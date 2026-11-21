#include "display.hpp"
#include "config.hpp"
#include "spotify.hpp"

#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <vector>

using Spotify::TrackInfo;

namespace {

TFT_eSPI tft;

// TJpg_Decoder calls this for each decoded block
bool jpg_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) {
        return false;
    }
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

}  // namespace

namespace Display {

void init() {
    tft.begin();
    tft.setRotation(1);  // Landscape (320x170)
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(jpg_output);
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.println("Initializing display...");
}
void render(const TrackInfo &track_info) {
    if (!track_info.success || track_info.title.isEmpty()) {
        Render::non_playing();
        return;
    }

    Render::all(track_info);
}

}  // namespace Display

namespace Display::Render::Detail {

// Formats seconds as "m:ss"
std::string format_mins_secs(unsigned long total_secs) {
    const unsigned long mins = total_secs / 60;
    const unsigned long secs = total_secs % 60;

    if (secs < 10) {
        return std::to_string(mins) + ":0" + std::to_string(secs);
    }

    return std::to_string(mins) + ':' + std::to_string(secs);
}

void invalidate_caches() {
    render_cache.title = "";
    render_cache.artists = "";
    render_cache.album_cover_url = "";
    render_cache.progress_secs = std::numeric_limits<unsigned long>::max();
    render_cache.duration_secs = std::numeric_limits<unsigned long>::max();
}

// Shortens text (track metadata) to fit inside the screen - appends "..." until it fits in
// max_width pixels. Measures with the current text size, so call it after setTextSize()
String fit_text(String text, int max_width) {
    if (tft.textWidth(text) <= max_width) {
        return text;
    }

    while (text.length() > 0 && tft.textWidth(text + "...") > max_width) {
        text.remove(text.length() - 1);
    }

    return text + "...";
}

}  // namespace Display::Render::Detail

namespace Display::Render {

void progress_bar(unsigned long progress_ms, unsigned long duration_ms) {
    // Avoid division by 0 (undefined behavior)
    // Intentionally not "duration_ms <= 0", because duration_ms is unsigned long
    if (duration_ms == 0) {
        return;
    }

    constexpr Rect outline{10, 120, 300, 20};
    constexpr Rect progress_bar_padded = outline.padded(10);  // Padding in %

    // Draw rectangled-shaped outline
    tft.drawRect(outline.x, outline.y, outline.w, outline.h, TFT_WHITE);

    const int filled_w = std::clamp<int>(
        static_cast<int>(static_cast<float>(progress_ms) / duration_ms * progress_bar_padded.w),
        0,
        progress_bar_padded.w);

    // Fill up progress bar progress out of duration
    tft.fillRect(
        progress_bar_padded.x, progress_bar_padded.y, filled_w, progress_bar_padded.h, TFT_GREEN);

    // Clear out remaining progress bar space (if any)
    // to prevent flashing of unstyled content (FOUC)
    tft.fillRect(progress_bar_padded.x + filled_w,
        progress_bar_padded.y,
        progress_bar_padded.w - filled_w,
        progress_bar_padded.h,
        TFT_BLACK);
}

void time_label(unsigned long progress_ms, unsigned long duration_ms) {
    constexpr Rect label_area{10, 146, 300, 16};

    auto &cache = Detail::render_cache;

    const unsigned long progress_secs = progress_ms / 1000;
    const unsigned long duration_secs = duration_ms / 1000;

    // Redraw only when the displayed text would change
    if (progress_secs == cache.progress_secs && duration_secs == cache.duration_secs) {
        return;
    }

    const std::string label_txt =
        Detail::format_mins_secs(progress_secs) + '/' + Detail::format_mins_secs(duration_secs);

    // Clear the whole area: the text width changes
    tft.fillRect(label_area.x, label_area.y, label_area.w, label_area.h, TFT_BLACK);

    tft.setTextWrap(false);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(label_area.x, label_area.y);
    tft.print(label_txt.c_str());

    cache.progress_secs = progress_secs;
    cache.duration_secs = duration_secs;
}

void album_cover(const String &url) {
    constexpr Rect outline{10, 10, 66, 66};
    constexpr Rect img{outline.x + 1, outline.y + 1, outline.w - 2, outline.h - 2};

    tft.drawRect(outline.x, outline.y, outline.w, outline.h, TFT_DARKGREY);

    auto &cache = Detail::render_cache;

    if (url.isEmpty()) {
        tft.fillRect(img.x, img.y, img.w, img.h, TFT_BLACK);
        cache.album_cover_url = "";
        return;
    }

    if (url == cache.album_cover_url) {
        return;
    }

    WiFiClientSecure client;
    Spotify::Detail::tls_certify_requests(client);

    HTTPClient http;
    if (!http.begin(client, url)) {
        return;
    }

    if (http.GET() != HTTP_CODE_OK) {
        tft.fillRect(img.x, img.y, img.w, img.h, TFT_BLACK);
        cache.album_cover_url = "";
        http.end();
        return;
    }

    const int len = http.getSize();
    if (len <= 0) {
        tft.fillRect(img.x, img.y, img.w, img.h, TFT_BLACK);
        cache.album_cover_url = "";
        http.end();
        return;
    }

    std::vector<uint8_t> jpg_buff(static_cast<std::size_t>(len));
    WiFiClient *stream = http.getStreamPtr();

    std::size_t bytes_read = 0;

    const auto start_time_ms = std::chrono::steady_clock::now();

    while (http.connected() && bytes_read < jpg_buff.size() &&
           std::chrono::steady_clock::now() - start_time_ms <
               Config::Display::ALBUM_COVER_DOWNLOAD_TIMEOUT_MS) {
        const int avail = stream->available();
        if (avail <= 0) {
            delay(1);
            continue;
        }

        const std::size_t to_read =
            std::min(static_cast<std::size_t>(avail), jpg_buff.size() - bytes_read);
        bytes_read += stream->readBytes(jpg_buff.data() + bytes_read, to_read);
        delay(1);
    }

    http.end();

    if (bytes_read != jpg_buff.size()) {
        tft.fillRect(img.x, img.y, img.w, img.h, TFT_BLACK);
        cache.album_cover_url = "";
        return;
    }

    TJpgDec.drawJpg(img.x, img.y, jpg_buff.data(), jpg_buff.size());
    cache.album_cover_url = url;
}

void non_playing() {
    if (Detail::render_cache.non_playing_shown) {
        return;
    }

    constexpr Rect label_position{160, 85, 0, 0};  // Screen center (320x170)

    tft.fillScreen(TFT_RED);

    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Nothing's playing", label_position.x, label_position.y);

    // Restore default for the other functions
    tft.setTextDatum(TL_DATUM);

    Detail::invalidate_caches();
    Detail::render_cache.non_playing_shown = true;
}

void metadata(const String &title, const String &artists) {
    constexpr Rect area{86, 10, 224, 66};

    auto &cache = Detail::render_cache;

    if (title == cache.title && artists == cache.artists) {
        return;
    }

    // Clear previous text inside this Rect only when song changes -
    // prevent flashing of unstyled content (FOUC)
    tft.fillRect(area.x, area.y, area.w, area.h, TFT_BLACK);

    // Prevent long titles from wrapping onto the next line or falling off-screen
    tft.setTextWrap(false);

    // Draw track name
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(area.x, area.y + 6);
    tft.print(Detail::fit_text(title, area.w));

    // Draw track artist/s name/s
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(area.x, area.y + 34);
    tft.print(Detail::fit_text(artists, area.w));

    cache.title = title;
    cache.artists = artists;
}

void all(const TrackInfo &track_info) {
    auto &cache = Detail::render_cache;

    // Wipe out non-playing display before rendering the track
    if (cache.non_playing_shown) {
        tft.fillScreen(TFT_BLACK);
        cache.non_playing_shown = false;
    }

    Render::metadata(track_info.title, track_info.artists);
    Render::album_cover(track_info.album_cover_url);
    Render::progress_bar(track_info.progress_ms, track_info.duration_ms);
    Render::time_label(track_info.progress_ms, track_info.duration_ms);
}

}  // namespace Display::Render
