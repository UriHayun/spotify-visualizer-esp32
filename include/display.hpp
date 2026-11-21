#pragma once

#include <spotify.hpp>

#include <algorithm>
#include <limits>
#include <string>

namespace Display {

void init();
void render(const Spotify::TrackInfo &track_info);

}  // namespace Display

namespace Display::Render::Detail {

struct RenderCache {
    String title;
    String artists;
    String album_cover_url;
    unsigned long progress_secs = std::numeric_limits<unsigned long>::max();
    unsigned long duration_secs = std::numeric_limits<unsigned long>::max();
    bool non_playing_shown = false;
};

inline RenderCache render_cache;

std::string format_mins_secs(unsigned long total_secs);

void invalidate_caches();

String fit_text(String text, int max_width);

}  // namespace Display::Render::Detail

namespace Display::Render {

struct Rect {
    // Coordinates on display
    const int x;
    const int y;

    // Size
    const int w;
    const int h;

    constexpr Rect padded(int padding_pct) const {
        const int padding = (std::min(w, h) * padding_pct) / 100;
        return {x + padding, y + padding, w - 2 * padding, h - 2 * padding};
    }
};

void progress_bar(unsigned long progress_ms, unsigned long duration_ms);
void time_label(unsigned long progress_ms, unsigned long duration_ms);
void album_cover(const String &url);
void non_playing();
void metadata(const String &title, const String &artists);
void all(const Spotify::TrackInfo &track_info);

}  // namespace Display::Render
