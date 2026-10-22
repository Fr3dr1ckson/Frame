#pragma once
#include <ESPAsyncWebServer.h>
#include <functional>
#include <vector>
#include <AsyncJson.h>
#include "WifiCaptivePage.h"
#include "wifi-types.h"

#define LocalIPURL "http://192.168.4.1"

struct WifiOperationCallbacks
{
    std::function<void()> resetSettings;
    std::function<void(const WifiCredentials, const String)> setConnectionCredentials;
    std::function<std::vector<WifiNetwork>(bool)> getAnnotatedWifiNetworks;
};

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP, WifiOperationCallbacks callbacks);
