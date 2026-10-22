#ifndef WiFiCaptive_h
#define WiFiCaptive_h

#include <AsyncTCP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <AsyncJson.h>
#include "Preferences.h"
#include "WifiCaptivePage.h"
#include <ArduinoJson.h>
#include "wifi-types.h"

#define WIFI_SSID "TRMNL"
#define WIFI_PASSWORD NULL

#define DNS_INTERVAL 60
#define MAX_CLIENTS 1
#define WIFI_CHANNEL 6
#define WIFI_MAX_SAVED_CREDS 5
#define WIFI_CONNECTION_ATTEMPTS 3
#define CONNECTION_TIMEOUT 15000

#define WIFI_SSID_KEY(i) ("wifi_" + String(i) + "_ssid").c_str()
#define WIFI_PSWD_KEY(i) ("wifi_" + String(i) + "_pswd").c_str()

#define WIFI_LAST_INDEX "wifi_last_index"

class WifiCaptive
{
private:
    DNSServer *_dnsServer;
    AsyncWebServer *_server;
    String _ssid = "";
    String _password = "";
    String _api_server = "";

    std::function<void()> _resetcallback;

    WifiCredentials _savedWifis[WIFI_MAX_SAVED_CREDS];

    void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP);
    uint8_t connect(const WifiCredentials credentials);
    uint8_t waitForConnectResult(uint32_t timeout);
    uint8_t waitForConnectResult();
    void readWifiCredentials();
    void saveWifiCredentials(const WifiCredentials credentials);
    void saveLastUsedWifiIndex(int index);
    int readLastUsedWifiIndex();
    void saveApiServer(String url);
    std::vector<WifiCredentials> matchWifiNetworks(std::vector<WifiNetwork> &scanResults, WifiCredentials wifiCredentials[]);
    std::vector<WifiNetwork> getScannedUniqueWifiNetworks(bool runScan);
    std::vector<WifiNetwork> combineWifiNetworks(std::vector<WifiNetwork> &scanResults, WifiCredentials wifiCredentials[]);
    bool tryConnectWithRetries(const WifiCredentials creds, int last_used_index);

public:
    /// @brief Starts WiFi configuration portal (AP mode with web UI).
    /// @return True if successfully connected to the provided SSID, false otherwise.
    bool startPortal();

    /// @brief Checks if any SSID is saved in NVS.
    bool isSaved();

    /// @brief Resets all saved WiFi credentials and API URL from NVS.
    void resetSettings();

    /// @brief Sets the callback triggered when user performs a soft reset via portal.
    void setResetSettingsCallback(std::function<void()> func);

    /// @brief Tries to connect to saved credentials, best signal first.
    /// @return True if successfully connected, false otherwise.
    bool autoConnect();
};

extern WifiCaptive WifiCaptivePortal;

#endif
