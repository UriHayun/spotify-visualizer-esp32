
#include "wifi.hpp"
#include "config.hpp"
#include "secrets.hpp"

#include <WiFi.h>
#include <WiFiManager.h>

#include <cstddef>

namespace Wifi {

void init() {
    // Set Wi-Fi mode to STA (Station) - wireless microcontroller acts as a client
    WiFi.mode(WIFI_STA);

    // Print found, nearby networks to Serial Monitor
    const int NETWORKS = WiFi.scanNetworks();
    Serial.printf("Found %d networks: ", NETWORKS);
    for (int i = 0; i < NETWORKS; i++) {
        const String ssid = WiFi.SSID(i);
        Serial.printf("%d: %s (%d dBm)\n", i + 1, ssid.c_str(), WiFi.RSSI(i));
    }

    WiFiManager wm;

    // Configure to auto-close the portal if nobody configures it within N seconds
    wm.setConfigPortalTimeout(Config::Wifi::RECONN_INTERVAL_MS);

    Serial.println("Attempting connection via WiFiManager...");

    bool success = wm.autoConnect("ESP32_Config_AP", Secrets::Wifi::AUTO_CONN_PASSWORD);

    if (!success) {
        Serial.println("Failed to connect and configuration portal timed out. Restarting...");
        delay(3000);
        ESP.restart();  // Restart the ESP32 to retry the connection cycle
    }

    // Enforce native background auto-reconnects once connected
    WiFi.setAutoReconnect(true);

    const String ip_address = WiFi.localIP().toString();
    Serial.printf("WiFi connected successfully!\nIP Address: %s\n", ip_address.c_str());
}

}  // namespace Wifi
