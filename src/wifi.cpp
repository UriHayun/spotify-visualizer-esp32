#include "wifi.hpp"
#include "secrets.h"

#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_sleep.h>

#include <cstddef>

namespace {

constexpr unsigned long CONFIG_PORTAL_TIMEOUT_SEC = 180;  // 3 minutes

}  // namespace

namespace Wifi {

void init() {
    // Set Wi-Fi mode to STA (Station) - wireless microcontroller acts as a client
    WiFi.mode(WIFI_STA);

    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);

    // Print found, nearby networks to Serial Monitor
    const int networks = WiFi.scanNetworks();
    if (networks < 0) {
        Serial.printf("Wi-Fi scan failed (error code: %d)\n", networks);
    } else {
        Serial.printf("Found %d networks:\n", networks);
        for (int i = 0; i < networks; i++) {
            const String ssid = WiFi.SSID(i);
            Serial.printf("%d: %s (%ld dBm)\n", i + 1, ssid.c_str(), WiFi.RSSI(i));
        }
    }

    WiFiManager wm;

    // Configure to auto-close the portal if nobody configures it within N seconds
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_SEC);

    Serial.println("Attempting connection via WiFiManager...");

    // - Try joining networks of saved credentials from a previous setup first.
    // - If none saved or the join fails: ESP32 switches to AP mode -
    //   hosts its own hotspot "ESP32_Config_AP".
    // - User connects using a phone, captive portal opens where you provide a
    //   Wi-Fi's SSID and password in order to connect to a router
    // * This Wi-Fi's credentials are saved to flash (1st step)
    // * If nobody configures before configured portal timeout, returns false
    // Blocks program until one of these outcomes occurs
    bool success = wm.autoConnect("ESP32_Config_AP", Secrets::Wifi::AUTO_CONN_PASSWORD);

    if (!success) {
        Serial.println("Wi-Fi not configured before the portal timed out. Shutting down...");
        Serial.flush();
        esp_deep_sleep_start();
    }

    // Enforce native background auto-reconnects once connected
    WiFi.setAutoReconnect(true);

    const String ip_address = WiFi.localIP().toString();
    Serial.printf("Wi-Fi connected successfully!\nIP Address: %s\n", ip_address.c_str());
    Serial.printf("Signal: %d dBm\n", WiFi.RSSI());
}

}  // namespace Wifi
