#pragma once
#include <Arduino.h>
// Minimal shim mapping ArduinoLog macros used by WifiCaptive to Serial.printf
#define Log_info(fmt, ...)    Serial.printf("[WIFI] " fmt "\n", ##__VA_ARGS__)
#define Log_verbose(fmt, ...) Serial.printf("[WIFI] " fmt "\n", ##__VA_ARGS__)
#define Log_error(fmt, ...)   Serial.printf("[WIFI ERR] " fmt "\n", ##__VA_ARGS__)
