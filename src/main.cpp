#include "buttons.hpp"
#include "display.hpp"
#include "spotify.hpp"
#include "wifi.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include <cstdint>

Spotify::TrackInfo curr_track_info;

namespace {

// Speed at which data is sent over a serial communication cable (bps)
constexpr unsigned long MONITOR_BAUD_RATE_BPS = 115'200;

std::uint32_t refetch_at_ms = 0;
constexpr unsigned long SERIAL_WAIT_TIMEOUT_MS = 3000;
constexpr unsigned long SPOTIFY_REFETCH_DELAY_MS = 400;

}  // namespace

void setup() {
    Serial.begin(MONITOR_BAUD_RATE_BPS);

    const std::uint32_t wait_start_ms = millis();
    while (!Serial && millis() - wait_start_ms < SERIAL_WAIT_TIMEOUT_MS) {
        delay(10);
    }

    Serial.println("System initialized. Start Spotify visualizer...");

    Display::init();
    Wifi::init();
    Buttons::init();

    // Sync clock via NTP so TLS certificate validation succeeds
    // (prevent 1970 Unix Epoch default time problem)
    if (Spotify::Detail::sync_device_ntp()) {
        Serial.println("Time synchronized successfully!");
    } else {
        Serial.println("NTP time sync failed. TLS certificate validation might fail");
    }

    // Explicitly cache current track info for loop()
    curr_track_info = Spotify::fetch_curr_track_info();

    Display::render(curr_track_info);
}

void loop() {
    // Continuously update currently playing track if connected to WiFi
    if (WiFi.status() == WL_CONNECTED) {
        Spotify::update_curr_track_info(curr_track_info);
        Display::render(curr_track_info);
    }

    switch (Buttons::poll_action()) {
    case Buttons::Action::PrevTapped:
        if (Spotify::skip_prev())
            refetch_at_ms = millis() + SPOTIFY_REFETCH_DELAY_MS;

        break;

    case Buttons::Action::NextTapped:
        if (Spotify::skip_next())
            refetch_at_ms = millis() + SPOTIFY_REFETCH_DELAY_MS;

        break;

    case Buttons::Action::PlayPauseRequested:
        if (curr_track_info.is_playing ? Spotify::pause_playback() : Spotify::play_playback()) {
            refetch_at_ms = millis() + SPOTIFY_REFETCH_DELAY_MS;
        }

        break;

    case Buttons::Action::None:
        break;
    }

    if (refetch_at_ms != 0 && millis() >= refetch_at_ms) {
        refetch_at_ms = 0;

        const Spotify::TrackInfo fresh = Spotify::fetch_curr_track_info();
        if (fresh.success) {
            curr_track_info = fresh;
        }
    }

    delay(10);
}
