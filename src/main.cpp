#include "config.hpp"
#include "display.hpp"
#include "spotify.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <WiFi.h>

Spotify::TrackInfo curr_track_info;

void setup() {
    Serial.begin(Config::Serial::MONITOR_BAUD_RATE_BPS);

    while (!Serial) {
        delay(10);
    }

    Serial.println("System initialized. Start Spotify visulizer...");

    Display::init();
    Wifi::init();

    // Sync clock via NTP so TLS certificate validation succeeds
    // (prevent 1970 Unix Epoch default time problem)
    if (Spotify::Detail::sync_device_ntp()) {
        Serial.println("NTP time sync successful");
    } else {
        Serial.println("NTP time sync failed. TLS certificate validation might fail");
    }

    Display::render(Spotify::fetch_curr_track_info);
}

void loop() {
    // Continuosly update currently playing track if connected to WiFi
    if (WiFi.status() == WL_CONNECTED) {
        Spotify::update_curr_track_info(curr_track_info);
    }

    delay(100);
}
