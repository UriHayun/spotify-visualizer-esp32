#include "spotify.hpp"
#include "config.hpp"
#include "esp_sntp.h"
#include "secrets.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <array>
#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace {

std::mutex token_mutex;
String token = "";
unsigned long token_expiry_ms = 0;
std::atomic<bool> is_ntp_synced = false;

template <typename RequestFn> int with_token_retry(RequestFn request) {
    String access_token = Spotify::Detail::fetch_access_token();
    if (access_token.isEmpty()) {
        Serial.println("No access token available");
        return -1;
    }

    int http_res_code = request(access_token);
    if (http_res_code != HTTP_CODE_UNAUTHORIZED) {
        return http_res_code;
    }

    Serial.println("HTTP 401: refreshing token and retrying once");
    access_token = Spotify::Detail::fetch_access_token(true);
    if (access_token.isEmpty()) {
        Serial.println("Token force-refresh returned empty token");
        return -1;
    }

    return request(access_token);
}

enum class PlayerCommand { Back, Forward };

constexpr std::string_view PLAYER_BASE_URL = "https://api.spotify.com/v1/me/player/";

constexpr std::string_view endpoint_path(PlayerCommand cmd) {
    switch (cmd) {
    case PlayerCommand::Back:
        return "previus";
    case PlayerCommand::Forward:
        return "next";
    }

    return {};
}

bool send_player_cmd(PlayerCommand cmd) {
    WiFiClientSecure client;

    // if (!Detail::tls_certify_requests(client)) {
    //     return false;
    // }

    client.setInsecure();  // TEMPORARY

    const std::string request_url = std::string(PLAYER_BASE_URL) + std::string(endpoint_path(cmd));

    const int http_res_code = with_token_retry([&](const String &access_token) {
        HTTPClient http;

        if (!http.begin(client, request_url.c_str())) {
            return -1;
        }

        http.addHeader("Authorization", "Bearer " + access_token);

        const int code = http.POST("");
        http.end();
        return code;
    });

    switch (http_res_code) {
    case HTTP_CODE_OK:
    case HTTP_CODE_NO_CONTENT:
        return true;
    case HTTP_CODE_FORBIDDEN:
        Serial.println("HTTP 403: access forbidden. Your Spotify access token has expired, is "
                       "invalid, or your account isn't premium");
        return false;
    case HTTP_CODE_NOT_FOUND:
        Serial.println("HTTP 404: no active device");
        return false;
    case HTTP_CODE_TOO_MANY_REQUESTS:
        Serial.println("HTTP 429: too many requests. Application has exceeded rate limits");
        return false;
    default:
        Serial.printf("Unexpected HTTP response code: %d\n", http_res_code);
        return false;
    }
}

void invalidate_token() {
    const std::lock_guard<std::mutex> lock(token_mutex);
    token = "";
    token_expiry_ms = 0;
}

}  // namespace

namespace Spotify {

TrackInfo fetch_curr_track_info() {
    TrackInfo track_info;
    WiFiClientSecure client;
    HTTPClient http;

    if (!Detail::tls_certify_requests(client)) {
        return track_info;
    }

    Serial.println("Fetching playing track from Spotify...");

    if (!http.begin(client,
            "https://api.spotify.com/v1/me/player/"
            "currently-playing?additional_types=track,episode")) {
        Serial.println("Failed to begin HTTPS session");
        return track_info;
    }

    String access_token = Detail::fetch_access_token();
    if (access_token.isEmpty()) {
        Serial.println("No access token available");
        http.end();
        return track_info;
    }

    String auth_header = "Bearer " + access_token;

    http.addHeader("Authorization", auth_header);
    http.addHeader("Content-Type", "application/json");

    int http_res_code = http.GET();

    // Intercept low-level network or hardware errors first
    if (http_res_code <= 0) {
        Serial.printf("HTTP %d: connection failed or blocked\n", http_res_code);
        http.end();
        return track_info;
    }

    // If access token is expired/invalid, force-refresh the token before
    if (http_res_code == HTTP_CODE_UNAUTHORIZED) {
        Serial.println("HTTP 401: unauthorized. "
                       "Your Spotify access token has expired or is invalid");
        http.end();

        access_token = Detail::fetch_access_token(true);
        if (access_token.isEmpty()) {
            Serial.println("Token force-refresh returned empty token");
            return track_info;
        }

        if (!http.begin(client,
                "https://api.spotify.com/v1/me/player/"
                "currently-playing?additional_types=track,episode")) {
            Serial.println("Failed to re-open HTTPS session after access token refresh");
            return track_info;
        }

        http.addHeader("Authorization", "Bearer " + access_token);
        http.addHeader("Content-Type", "application/json");
        http_res_code = http.GET();
    }

    // Intercept low-level network or hardware errors first
    if (http_res_code <= 0) {
        Serial.printf("HTTP %d: connection failed or blocked\n", http_res_code);
        http.end();
        return track_info;
    }

    switch (http_res_code) {
    case (HTTP_CODE_OK): {
        const String payload = http.getString();
        Serial.printf("Received response from Spotify API: %s\n", payload.c_str());

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (err) {
            Serial.printf("Failed to parse JSON: %s\n", err.c_str());
            http.end();
            return track_info;
        }

        track_info.is_playing = doc["is_playing"] | false;

        const unsigned long progress_ms = doc["progress_ms"] | 0UL;

        if (!doc["item"].isNull()) {
            const JsonObjectConst item = doc["item"];
            const bool is_episode = item["type"] == "episode";

            // Parse track information from the JSON response
            track_info.title = item["name"].as<String>();
            track_info.artists = is_episode ? item["show"]["name"].as<String>()
                                            : item["artists"][0]["name"].as<String>();
            // Album cover URL (images array is ordered by size - biggest to smallest)
            JsonArrayConst imgs = is_episode ? item["images"].as<JsonArrayConst>()
                                             : item["album"]["images"].as<JsonArrayConst>();
            if (imgs.isNull() || imgs.size() == 0) {
                track_info.album_cover_url = "";
                http.end();
                return track_info;
            }
            track_info.album_cover_url = imgs[imgs.size() - 1]["url"].as<String>();

            track_info.progress_ms = progress_ms;
            track_info.duration_ms = doc["item"]["duration_ms"].as<unsigned long>();
            track_info.success = true;
        } else {
            Serial.println("There was no active playback found (track data is NULL)");
        }

        break;
    }

    case (HTTP_CODE_NO_CONTENT):
        Serial.println("HTTP 204: no content. Connected successfully, no track is playing "
                       "right now though");
        break;

    case (HTTP_CODE_UNAUTHORIZED):
        // Reached only if the initial request and the forced token refresh both returned HTTP 401
        Serial.println(
            "HTTP 401: still unauthorized after token refresh - giving up for this poll");
        break;

    case (HTTP_CODE_FORBIDDEN):
        Serial.println("HTTP 403: access forbidden. Your Spotify access token has expired, is "
                       "invalid, or your account isn't premium");
        break;
    case (HTTP_CODE_TOO_MANY_REQUESTS):
        Serial.println("HTTP 429: too many requests. Application has exceeded rate limits");
        break;
    default:
        Serial.printf("Unexpected HTTP response code: %d\n", http_res_code);
        break;
    }

    http.end();
    return track_info;
}

void update_curr_track_info(TrackInfo &curr_track_info) {
    static unsigned long last_spotify_poll = 0;

    const unsigned long now_ms = millis();
    if (WiFi.status() == WL_CONNECTED &&
        now_ms - last_spotify_poll >= Config::Spotify::SPOTIFY_POLL_INTERVAL_MS) {
        last_spotify_poll = now_ms;
        curr_track_info = fetch_curr_track_info();
    }
}

bool skip_next() { return send_player_cmd(PlayerCommand::Forward); }
bool skip_previous() { return send_player_cmd(PlayerCommand::Back); }

}  // namespace Spotify

namespace Spotify::Detail {

// Solves Unix Epoch default time problem by syncing the time with NTP,
// in order to validate TLS certificates for HTTPS requests (prevents 1970 clock default error)
bool sync_device_ntp() {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    // Cannot resume if not connected to Wi-Fi
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    is_ntp_synced.store(false, std::memory_order_relaxed);
    esp_sntp_set_time_sync_notification_cb([](struct timeval *) {
        is_ntp_synced.store(true, std::memory_order_release);
        Serial.println("SNTP time synchronized via callback");
    });

    if (!esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, Config::Spotify::NTP_SERVER.data());
        esp_sntp_init();
    } else {
        esp_sntp_restart();  // Already running - force a fresh request
    }

    const auto start = steady_clock::now();
    Serial.print("Waiting for NTP time sync: ");

    while (!is_ntp_synced.load(std::memory_order_acquire)) {
        if (steady_clock::now() - start >= Config::Spotify::NTP_SYNC_TIMEOUT_MS) {
            Serial.println();
            return false;
        }
        Serial.print('.');
        std::this_thread::sleep_for(500ms);
    }

    Serial.println();
    Serial.println("Time synchronized successfully!");
    return true;
}

bool tls_certify_requests(WiFiClientSecure &client) {
    if (Config::Spotify::ROOT_CA[0] == '\0') {
        Serial.println("Spotify root CA certificate isn't configured");
        Serial.println(Config::Spotify::ROOT_CA);
        return false;
    }

    // client.setCACert(Config::Spotify::ROOT_CA);
    client.setInsecure();

    return true;
}

String fetch_access_token(bool force_refresh) {
    {
        const std::lock_guard<std::mutex> lock(token_mutex);
        const unsigned long now_ms = millis();
        const bool has_token = token.length() > 0;
        const bool is_unexpired = has_token && (now_ms < token_expiry_ms);

        if (!force_refresh && is_unexpired) {
            return token;
        }
    }

    WiFiClientSecure client;
    if (!tls_certify_requests(client)) {
        return "";
    }

    HTTPClient http;
    if (!http.begin(client, "https://accounts.spotify.com/api/token")) {
        Serial.println("Failed to begin token refresh HTTPS session");
        return "";
    }

    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    const String payload = String(
        std::format("grant_type=refresh_token&refresh_token={}&client_id={}&client_secret={}",
            Secrets::Spotify::REFRESH_TOKEN,
            Secrets::Spotify::CLIENT_ID,
            Secrets::Spotify::CLIENT_SECRET)
            .c_str());

    int http_code = http.POST(payload);

    if (http_code <= 0) {
        std::array<char, 128> tls_err{};
        client.lastError(tls_err.data(), tls_err.size());
        Serial.print("TLS error: ");
        Serial.println(tls_err.data());
    }

    const String body = (http_code == HTTP_CODE_OK) ? http.getString() : "";
    http.end();

    if (http_code != HTTP_CODE_OK) {
        Serial.printf("Failed to refresh access token! HTTP code: %d\n", http_code);
        invalidate_token();
        return "";
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, body);
    const String new_token = doc["access_token"] | "";

    if (err || new_token.length() == 0) {
        Serial.printf("Failed to parse token response: %s\n", err.c_str());
        invalidate_token();
        return "";
    }

    const unsigned long lifetime_ms = (doc["expires_in"] | 3600UL) * 1000UL;
    const unsigned long effective_ms =
        (lifetime_ms > Config::Spotify::REFRESH_MARGIN_MS)
            ? (lifetime_ms - Config::Spotify::REFRESH_MARGIN_MS)
            : (lifetime_ms / Config::Spotify::SHORT_LIVED_TOKEN_DIVISOR);

    const std::lock_guard<std::mutex> lock(token_mutex);
    token = new_token;
    token_expiry_ms = millis() + effective_ms;

    return token;
}

}  // namespace Spotify::Detail
