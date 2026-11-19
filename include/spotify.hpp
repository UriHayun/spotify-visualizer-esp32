#include <cstddef>
#include <string_view>

#include <Arduino.h>

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

}  // namespace Spotify

namespace Spotify::Detail {

bool sync_device_ntp();
bool tls_certify_requests(WiFiClientSecure &client);
String fetch_access_token(bool force_refresh = false);

}  // namespace Spotify::Detail
