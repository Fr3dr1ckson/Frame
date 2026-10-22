#include "WebServer.h"

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP, WifiOperationCallbacks callbacks)
{
    server.on("/wpad.dat", [](AsyncWebServerRequest *request)
              { request->send(404); });
    server.on("/favicon.ico", [](AsyncWebServerRequest *request)
              { request->send(404); });

    // ── Captive portal detection — all OS/browser combinations ──────────────
    // Each OS probes a known URL to check for internet. We redirect to the
    // portal page, which tells the OS "captive portal" and triggers the popup.

    // iOS 14+ RFC 8908 — highest priority, opened CNA directly to user-portal-url
    server.on("/api/v1/hotspot-detect", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        Serial.printf("[CND] iOS RFC8908 probe\n");
        AsyncWebServerResponse *r = request->beginResponse(200,
            "application/captive+json",
            "{\"captive\":true,\"user-portal-url\":\"" LocalIPURL "\"}");
        r->addHeader("Cache-Control", "no-store");
        request->send(r); });

    // iOS legacy
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request)
              {
        Serial.printf("[CND] iOS legacy probe\n");
        request->redirect(LocalIPURL); });
    server.on("/library/test/success.html", [](AsyncWebServerRequest *request)
              { request->redirect(LocalIPURL); });

    // Android — redirect triggers "Sign in to network" notification
    server.on("/generate_204", [](AsyncWebServerRequest *request)
              {
        Serial.printf("[CND] Android generate_204 probe\n");
        request->redirect(LocalIPURL); });
    server.on("/gen_204", [](AsyncWebServerRequest *request)
              { request->redirect(LocalIPURL); });

    // Windows
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request)
              { request->redirect("http://logout.net"); });
    server.on("/ncsi.txt", [](AsyncWebServerRequest *request)
              { request->redirect(LocalIPURL); });
    server.on("/redirect", [](AsyncWebServerRequest *request)
              { request->redirect(LocalIPURL); });

    // Firefox
    server.on("/canonical.html", [](AsyncWebServerRequest *request)
              { request->redirect(LocalIPURL); });
    server.on("/success.txt", [](AsyncWebServerRequest *request)
              { request->send(200); });

    // Serve the portal page (gzipped)
    server.on("/", HTTP_ANY, [&](AsyncWebServerRequest *request)
              {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", INDEX_HTML, INDEX_HTML_LEN);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response); });

    server.on("/logo.svg", HTTP_ANY, [&](AsyncWebServerRequest *request)
              {
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", LOGO_SVG, LOGO_SVG_LEN);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Content-Type", "image/svg+xml");
        request->send(response); });

    server.on("/soft-reset", HTTP_ANY, [callbacks](AsyncWebServerRequest *request)
              {
        callbacks.resetSettings();
        request->send(200); });

    server.on("/scan", HTTP_GET, [callbacks](AsyncWebServerRequest *request)
              {
        String json = "[";
        int n = WiFi.scanComplete();
        Serial.printf("[SCAN] scanComplete()=%d  WiFiMode=%d  AP_IP=%s\n",
                      n, WiFi.getMode(), WiFi.softAPIP().toString().c_str());
        if (n == WIFI_SCAN_FAILED) {
            // Do NOT call scanNetworks() here — async scan causes channel-hop in APSTA → drops AP.
            // Return the cached results (or empty) and let the browser retry.
            Serial.printf("[SCAN] scan failed — returning cached results, NOT triggering rescan\n");
        } else if (n == WIFI_SCAN_RUNNING) {
            return request->send(202);
        }
        if (n > 0) {
            std::vector<WifiNetwork> combinedWifiNetworks = callbacks.getAnnotatedWifiNetworks(false);
            size_t size = 0;
            for (const auto &network : combinedWifiNetworks)
            {
                String ssid = network.ssid;
                String rssi = String(network.rssi);
                ssid.replace("\\", "\\\\");
                ssid.replace("\"", "\\\"");
                json += "{";
                json += "\"name\":\"" + ssid + "\",";
                json += "\"rssi\":\"" + rssi + "\",";
                json += "\"open\":" + String(network.open == WIFI_AUTH_OPEN ? "true," : "false,");
                json += "\"saved\":" + String(network.saved ? "true" : "false");
                json += "}";
                size += 1;
                if (size != combinedWifiNetworks.size())
                    json += ",";
            }
            // Do NOT call scanDelete() or scanNetworks() here.
            // Triggering a new scan in APSTA mode causes channel-hopping that drops the AP.
            // Scan results from portal startup are reused for the whole session.
        }
        json += "]";
        request->send(200, "application/json", json);
        json = String(); });

    AsyncCallbackJsonWebHandler *handler = new AsyncCallbackJsonWebHandler("/connect", [callbacks](AsyncWebServerRequest *request, JsonVariant &json)
                                                                           {
        JsonObject data = json.as<JsonObject>();
        String ssid = data["ssid"];
        String pswd = data["pswd"];
        String api_server = data["server"];
        callbacks.setConnectionCredentials({ssid, pswd}, api_server);
        String mac = WiFi.macAddress();
        String message = "{\"ssid\":\"" + ssid + "\",\"mac\":\"" + mac + "\"}";
        request->send(200, "application/json", message); });

    server.addHandler(handler);

    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        Serial.printf("[HTTP] %s http://%s%s\n",
            request->methodToString(),
            request->host().c_str(),
            request->url().c_str());
        request->redirect(LocalIPURL); });
}
