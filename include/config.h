#pragma once

// WiFi credentials
#define IMG_FETCH_WIFI_SSID  "SSID"
#define IMG_FETCH_WIFI_PASS  "Password"

// TRMNL cloud server
#define TRMNL_BASE_URL       "https://trmnl.app"
#define TRMNL_FW_VERSION     "2.0.0"
#define TRMNL_MODEL          "TRMNL_V3_C5"

// Deep sleep duration used when the server does not provide a refresh_rate.
// Also the minimum enforced sleep (prevents hammering the server).
#define SLEEP_DURATION_SECONDS   900u   // 15 minutes
#define SLEEP_DURATION_MIN_SECS   60u   // never sleep less than 60 s
