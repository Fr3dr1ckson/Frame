#include "WifiCaptive.h"
#include <trmnl_log.h>
#include "WebServer.h"

void WifiCaptive::setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP)
{
    dnsServer.setTTL(3600);
    dnsServer.start(53, "*", localIP);
}

bool WifiCaptive::startPortal()
{
    // ── WiFi event logger — prints every state change to Serial ──────────────
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_AP_START:
                Serial.printf("[EVT] AP_START  channel=%d  IP=%s\n",
                    WiFi.channel(), WiFi.softAPIP().toString().c_str());
                break;
            case ARDUINO_EVENT_WIFI_AP_STOP:
                Serial.printf("[EVT] AP_STOP\n");
                break;
            case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
                Serial.printf("[EVT] AP_STA_CONNECTED  aid=%d\n", info.wifi_ap_staconnected.aid);
                break;
            case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
                Serial.printf("[EVT] AP_STA_DISCONNECTED  aid=%d  reason=%d\n",
                    info.wifi_ap_stadisconnected.aid, info.wifi_ap_stadisconnected.reason);
                break;
            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.printf("[EVT] STA_CONNECTED  ssid=%.*s  channel=%d\n",
                    info.wifi_sta_connected.ssid_len, info.wifi_sta_connected.ssid,
                    info.wifi_sta_connected.channel);
                break;
            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                Serial.printf("[EVT] STA_DISCONNECTED  reason=%d\n",
                    info.wifi_sta_disconnected.reason);
                break;
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                Serial.printf("[EVT] STA_GOT_IP  ip=%s\n",
                    IPAddress(info.got_ip.ip_info.ip.addr).toString().c_str());
                break;
            case ARDUINO_EVENT_WIFI_SCAN_DONE:
                Serial.printf("[EVT] SCAN_DONE  status=%d  count=%d\n",
                    info.wifi_scan_done.status, info.wifi_scan_done.number);
                break;
            default:
                break;
        }
    });

    _dnsServer = new DNSServer();
    _server    = new AsyncWebServer(80);

    // Standard private AP IP — avoids public-range routing quirks on phones.
    // 192.168.4.1 is also the ESP32 default, so AP_START fires with the right IP
    // immediately (no reconfigure race with DHCP).
    const IPAddress subnetMask(255, 255, 255, 0);
    const IPAddress localIP(192, 168, 4, 1);
    const IPAddress gatewayIP(192, 168, 4, 1);

    // Port 443 listener — iOS 14+ probes https://api.apple.com/api/v1/hotspot-detect
    // first. If port 443 refuses, it does NOT fall back to HTTP and the CNA never opens.
    // Accepting the TCP connection and replying with an HTTP response (not valid TLS)
    // lets the OS see a captive portal exists and fall back to the HTTP portal.
    AsyncWebServer *server443 = new AsyncWebServer(443);
    server443->onNotFound([](AsyncWebServerRequest *request) {
        Serial.printf("[443] probe: %s http://%s%s\n",
            request->methodToString(), request->host().c_str(), request->url().c_str());
        AsyncWebServerResponse *r = request->beginResponse(302, "text/plain", "");
        r->addHeader("Location", LocalIPURL);
        request->send(r);
    });

    // Register web-server routes once — they survive begin()/end() cycles.
    WifiOperationCallbacks callbacks = {
        .resetSettings = [this]()
        {
            resetSettings();
            if (_resetcallback != NULL)
                _resetcallback();
        },
        .setConnectionCredentials = [this](const WifiCredentials credentials, const String api_server)
        {
            _ssid     = credentials.ssid;
            _password = credentials.pswd;
            _api_server = api_server;
        },
        .getAnnotatedWifiNetworks = [this](bool runScan)
        {
            std::vector<WifiNetwork> unique   = getScannedUniqueWifiNetworks(false);
            std::vector<WifiNetwork> combined = combineWifiNetworks(unique, _savedWifis);
            return combined;
        }};
    setUpWebserver(*_server, localIP, callbacks);

    bool succesfullyConnected = false;

    // Outer loop: restart AP after a failed connection attempt so user can retry.
    while (true)
    {
        _ssid     = "";
        _password = "";

        // ── Start AP ────────────────────────────────────────────────────────
        Log_info("--- Starting AP ---");
        WiFi.disconnect(true);
        Log_info("disconnect(true) done  mode=%d", WiFi.getMode());
        WiFi.mode(WIFI_OFF);
        delay(200);
        Log_info("WIFI_OFF done");
        WiFi.mode(WIFI_MODE_APSTA);
        delay(100);
        Log_info("WIFI_MODE_APSTA set  mode=%d", WiFi.getMode());

        // Scan BEFORE the AP starts so there is no AP to disrupt during channel-hopping.
        // In APSTA mode, scanning forces the radio off the AP channel → AP drops.
        // Doing it here in pure APSTA-pre-AP state avoids that completely.
        WiFi.scanDelete();
        int numNets = WiFi.scanNetworks(false);
        Log_info("Pre-AP scan done: %d networks", numNets);
        for (int i = 0; i < numNets; i++) {
            Log_info("  [%d] ssid='%s' ch=%d rssi=%d", i,
                     WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
        }

        // C5 is dual-band; pin the radio to 2.4 GHz so the AP on ch 6 is what phones see.
        Log_info("setBandMode(2G) returned %d", WiFi.setBandMode(WIFI_BAND_MODE_2G_ONLY));

        WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
        bool apOk = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, 4);
        delay(300);
        Log_info("softAP() returned %d  channel=%d  AP_IP=%s  heap=%u",
                 apOk, WiFi.channel(), WiFi.softAPIP().toString().c_str(), ESP.getFreeHeap());

        esp_wifi_set_ps(WIFI_PS_NONE);          // no power-save = stable beacons
        WiFi.setTxPower(WIFI_POWER_19_5dBm);

        setUpDNSServer(*_dnsServer, localIP);
        Log_info("DNS server started");
        _server->begin();
        server443->begin();
        Log_info("Web server started (80+443)  heap=%u", ESP.getFreeHeap());

        readWifiCredentials();

        Log_info("Portal up — connect to SSID '%s', open %s", WIFI_SSID, LocalIPURL);

        // ── Wait for credentials ─────────────────────────────────────────────
        uint32_t heartbeatTick = 0;
        while (_ssid == "")
        {
            _dnsServer->processNextRequest();
            delay(DNS_INTERVAL);
            // Heartbeat every ~5 s so we can see in Serial that the AP is still alive.
            if (++heartbeatTick % (5000 / DNS_INTERVAL) == 0) {
                Log_info("[heartbeat] mode=%d  AP_IP=%s  softAPStations=%d  heap=%u",
                         WiFi.getMode(),
                         WiFi.softAPIP().toString().c_str(),
                         WiFi.softAPgetStationNum(),
                         ESP.getFreeHeap());
            }
        }

        // ── Tear down AP before connecting ───────────────────────────────────
        // Find which channel the target network is on so we can log the conflict.
        int targetCh = -1;
        int n = WiFi.scanComplete();
        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == _ssid) { targetCh = WiFi.channel(i); break; }
        }
        Log_info("Credentials for '%s' (target ch=%d, AP ch=%d) — waiting 1s then tearing down AP",
                 _ssid.c_str(), targetCh, WiFi.channel());
        delay(1000);

        _server->end();
        server443->end();
        Log_info("Web servers stopped");
        _dnsServer->stop();
        Log_info("DNS server stopped");
        WiFi.softAPdisconnect(true);
        WiFi.scanDelete();
        delay(300);
        Log_info("AP torn down  mode=%d", WiFi.getMode());

        // ── Attempt STA connection in pure STA mode ──────────────────────────
        WiFi.mode(WIFI_STA);
        Log_info("Switched to WIFI_STA  mode=%d", WiFi.getMode());
        WifiCredentials credentials = {_ssid, _password};
        bool res = connect(credentials) == WL_CONNECTED;

        if (res)
        {
            saveWifiCredentials(credentials);
            saveApiServer(_api_server);
            succesfullyConnected = true;
            break;
        }

        // Wrong password or network gone — loop back and restart the portal.
        Log_info("Connection to '%s' failed — restarting portal for retry", _ssid.c_str());
    }

    delete _dnsServer;
    _dnsServer = nullptr;
    delete _server;
    _server = nullptr;
    delete server443;

    return succesfullyConnected;
}

void WifiCaptive::resetSettings()
{
    Preferences preferences;
    preferences.begin("wificaptive", false);
    preferences.remove("api_url");
    preferences.remove(WIFI_LAST_INDEX);
    for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
    {
        preferences.remove(WIFI_SSID_KEY(i));
        preferences.remove(WIFI_PSWD_KEY(i));
    }
    preferences.end();

    for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
        _savedWifis[i] = {"", ""};

    WiFi.disconnect(true, true);
}

uint8_t WifiCaptive::connect(const WifiCredentials credentials)
{
    uint8_t connRes = (uint8_t)WL_NO_SSID_AVAIL;

    if (credentials.ssid != "")
    {
        WiFi.begin(credentials.ssid.c_str(), credentials.pswd.c_str());
        connRes = waitForConnectResult();
    }

    return connRes;
}

uint8_t WifiCaptive::waitForConnectResult(uint32_t timeout)
{
    if (timeout == 0)
        return WiFi.waitForConnectResult();

    unsigned long timeoutmillis = millis() + timeout;
    uint8_t status = WiFi.status();

    while (millis() < timeoutmillis)
    {
        status = WiFi.status();
        if (status == WL_CONNECTED || status == WL_CONNECT_FAILED)
            return status;
        delay(100);
    }

    return status;
}

uint8_t WifiCaptive::waitForConnectResult()
{
    return waitForConnectResult(CONNECTION_TIMEOUT);
}

void WifiCaptive::setResetSettingsCallback(std::function<void()> func)
{
    _resetcallback = func;
}

bool WifiCaptive::isSaved()
{
    readWifiCredentials();
    return _savedWifis[0].ssid != "";
}

void WifiCaptive::readWifiCredentials()
{
    Preferences preferences;
    preferences.begin("wificaptive", false); // false = create namespace if first boot

    for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
    {
        _savedWifis[i].ssid = preferences.getString(WIFI_SSID_KEY(i), "");
        _savedWifis[i].pswd = preferences.getString(WIFI_PSWD_KEY(i), "");
    }

    preferences.end();
}

void WifiCaptive::saveWifiCredentials(const WifiCredentials credentials)
{
    Log_info("Saving wifi credentials: %s", credentials.ssid.c_str());

    for (u16_t i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
    {
        if (_savedWifis[i].ssid == credentials.ssid && _savedWifis[i].pswd == credentials.pswd)
            return;
    }

    for (u16_t i = WIFI_MAX_SAVED_CREDS - 1; i > 0; i--)
        _savedWifis[i] = _savedWifis[i - 1];

    _savedWifis[0] = credentials;

    Preferences preferences;
    preferences.begin("wificaptive", false);
    for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
    {
        preferences.putString(WIFI_SSID_KEY(i), _savedWifis[i].ssid);
        preferences.putString(WIFI_PSWD_KEY(i), _savedWifis[i].pswd);
    }
    preferences.putInt(WIFI_LAST_INDEX, 0);
    preferences.end();
}

void WifiCaptive::saveLastUsedWifiIndex(int index)
{
    Preferences preferences;
    preferences.begin("wificaptive", false);

    if (index < 0 || index >= WIFI_MAX_SAVED_CREDS)
        index = 0;

    if (index > 0)
    {
        readWifiCredentials();
        if (_savedWifis[index].ssid == "")
            index = 0;
    }

    preferences.putInt(WIFI_LAST_INDEX, index);
}

int WifiCaptive::readLastUsedWifiIndex()
{
    Preferences preferences;
    preferences.begin("wificaptive", true);
    int index = preferences.getInt(WIFI_LAST_INDEX, 0);
    if (index < 0 || index >= WIFI_MAX_SAVED_CREDS)
        index = 0;

    if (index > 0)
    {
        readWifiCredentials();
        if (_savedWifis[index].ssid == "")
            index = 0;
    }
    preferences.end();
    return index;
}

void WifiCaptive::saveApiServer(String url)
{
    if (url == "")
        return;
    Preferences preferences;
    preferences.begin("data", false);
    preferences.putString("api_url", url);
    preferences.end();
}

std::vector<WifiNetwork> WifiCaptive::getScannedUniqueWifiNetworks(bool runScan)
{
    std::vector<WifiNetwork> uniqueWifiNetworks;
    int n = WiFi.scanComplete();
    if (runScan == true)
    {
        WiFi.scanNetworks(false);
        delay(100);
        n = WiFi.scanComplete();
        while (n == WIFI_SCAN_RUNNING || n == WIFI_SCAN_FAILED)
        {
            delay(100);
            if (n == WIFI_SCAN_RUNNING)
            {
                n = WiFi.scanComplete();
            }
            else if (n == WIFI_SCAN_FAILED)
            {
                Log_verbose("Scan failed, waiting 10s before retry");
                delay(10000);
                n = WiFi.scanComplete();
                if (n > 0)
                {
                    Log_verbose("Scan completed with %d networks", n);
                    break;
                }
                WiFi.scanNetworks(false);
                delay(500);
                n = WiFi.scanComplete();
            }
        }
    }

    n = WiFi.scanComplete();
    Log_verbose("Final scan result: %d networks", n);

    for (int i = 0; i < n; ++i)
    {
        if (!WiFi.SSID(i).equals("TRMNL"))
        {
            String ssid = WiFi.SSID(i);
            int32_t rssi = WiFi.RSSI(i);
            bool open = WiFi.encryptionType(i);
            bool found = false;
            for (auto &network : uniqueWifiNetworks)
            {
                if (network.ssid == ssid)
                {
                    found = true;
                    if (network.rssi < rssi)
                        network.rssi = rssi;
                    break;
                }
            }
            if (!found)
                uniqueWifiNetworks.push_back({ssid, rssi, open});
        }
    }

    Log_info("Unique networks found: %d", uniqueWifiNetworks.size());
    return uniqueWifiNetworks;
}

std::vector<WifiCredentials> WifiCaptive::matchWifiNetworks(
    std::vector<WifiNetwork> &scanResults,
    WifiCredentials savedWifis[])
{
    std::sort(scanResults.begin(), scanResults.end(), [](const WifiNetwork &a, const WifiNetwork &b)
              { return a.rssi > b.rssi; });

    std::vector<WifiCredentials> sortedWifis;
    for (auto &network : scanResults)
    {
        for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
        {
            if (network.ssid == savedWifis[i].ssid)
                sortedWifis.push_back(savedWifis[i]);
        }
    }
    return sortedWifis;
}

std::vector<WifiNetwork> WifiCaptive::combineWifiNetworks(
    std::vector<WifiNetwork> &scanResults,
    WifiCredentials savedWifis[])
{
    std::vector<WifiNetwork> combinedWifiNetworks;
    for (auto &network : scanResults)
    {
        bool found = false;
        for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
        {
            if (network.ssid == savedWifis[i].ssid)
            {
                combinedWifiNetworks.push_back({network.ssid, network.rssi, network.open, true});
                found = true;
                break;
            }
        }
        if (!found)
            combinedWifiNetworks.push_back({network.ssid, network.rssi, network.open, false});
    }

    for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
    {
        bool found = false;
        for (auto &network : combinedWifiNetworks)
        {
            if (network.ssid == savedWifis[i].ssid)
            {
                found = true;
                break;
            }
        }
        if (!found && savedWifis[i].ssid != "")
            combinedWifiNetworks.push_back({savedWifis[i].ssid, -200, false, true});
    }

    return combinedWifiNetworks;
}

bool WifiCaptive::autoConnect()
{
    Log_info("Trying to autoconnect to wifi...");
    readWifiCredentials();

    int last_used_index = readLastUsedWifiIndex();

    if (_savedWifis[last_used_index].ssid != "")
    {
        Log_info("Trying to connect to last used: %s", _savedWifis[last_used_index].ssid.c_str());
        WiFi.setSleep(0);
        WiFi.mode(WIFI_STA);

        if (tryConnectWithRetries(_savedWifis[last_used_index], last_used_index))
            return true;
    }

    Log_info("Last used network unavailable, scanning for known networks...");
    std::vector<WifiNetwork> scanResults = getScannedUniqueWifiNetworks(true);
    std::vector<WifiCredentials> sortedWifiNetworks = matchWifiNetworks(scanResults, _savedWifis);
    if (sortedWifiNetworks.size() == 0)
    {
        Log_info("No matched networks found, trying all saved networks...");
        sortedWifiNetworks = std::vector<WifiCredentials>(_savedWifis, _savedWifis + WIFI_MAX_SAVED_CREDS);
    }

    WiFi.mode(WIFI_STA);
    for (auto &network : sortedWifiNetworks)
    {
        if (network.ssid == "" || (network.ssid == _savedWifis[last_used_index].ssid && network.pswd == _savedWifis[last_used_index].pswd))
            continue;

        Log_info("Trying saved network: %s", network.ssid.c_str());
        int found_index = -1;
        for (int i = 0; i < WIFI_MAX_SAVED_CREDS; i++)
        {
            if (_savedWifis[i].ssid == network.ssid)
            {
                found_index = i;
                break;
            }
        }
        if (tryConnectWithRetries(network, found_index))
            return true;
    }

    Log_info("Failed to connect to any saved network");
    return false;
}

bool WifiCaptive::tryConnectWithRetries(const WifiCredentials creds, int last_used_index)
{
    for (int attempt = 0; attempt < WIFI_CONNECTION_ATTEMPTS; attempt++)
    {
        Log_info("Attempt %d/%d for %s", attempt + 1, WIFI_CONNECTION_ATTEMPTS, creds.ssid.c_str());
        connect(creds);
        if (WiFi.status() == WL_CONNECTED)
        {
            Log_info("Connected to %s", creds.ssid.c_str());
            if (last_used_index >= 0)
                saveLastUsedWifiIndex(last_used_index);
            return true;
        }
        WiFi.disconnect();
        if (attempt < WIFI_CONNECTION_ATTEMPTS - 1)
        {
            uint32_t backoff_delay = 2000 * (1 << attempt);
            Log_info("Failed, retrying in %d ms", backoff_delay);
            delay(backoff_delay);
        }
    }
    return false;
}

WifiCaptive WifiCaptivePortal;
