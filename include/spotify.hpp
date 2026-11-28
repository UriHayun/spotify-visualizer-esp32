#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>

#include <cstddef>
#include <string_view>

namespace Spotify {

struct TrackInfo {
    String title;
    String artists;
    String album_cover_url;
    unsigned long progress_ms = 0;
    unsigned long duration_ms = 0;
    bool is_playing = false;
    bool success = false;
};

TrackInfo fetch_curr_track_info();
void update_curr_track_info(TrackInfo &current_track);

[[nodiscard]] bool skip_next();
[[nodiscard]] bool skip_prev();

[[nodiscard]] bool play_playback();
[[nodiscard]] bool pause_playback();

}  // namespace Spotify

namespace Spotify::Detail {

bool sync_device_ntp();
bool configure_tls(WiFiClientSecure &client);

}  // namespace Spotify::Detail
