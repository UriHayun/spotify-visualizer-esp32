#include "spotify.hpp"
#include "esp_sntp.h"
#include "secrets.h"

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

using namespace std::chrono_literals;

std::mutex token_mutex;
String token = "";
unsigned long token_acquired_ms = 0;
unsigned long token_effective_lifetime_ms = 0;
std::atomic<bool> is_ntp_synced = false;
constexpr unsigned long SPOTIFY_POLL_INTERVAL_MS = 4000;
constexpr char NTP_SERVER[] = "pool.ntp.org";
constexpr std::string_view SPOTIFY_PLAYER_ENDPOINT = "https://api.spotify.com/v1/me/player/";
constexpr std::chrono::milliseconds NTP_SYNC_TIMEOUT_MS = 20'000ms;
constexpr unsigned long REFRESH_MARGIN_MS = 5UL * 60 * 1000;  // Refresh 5 minutes early
constexpr unsigned long SHORT_LIVED_TOKEN_DIVISOR = 2;

// PEM-encoded CA certificate trusted to validate api.spotify.com's TLS certificate chain
// Include the "-----BEGIN CERTIFICATE-----" and "-----END CERTIFICATE-----" lines
// Keep certificate verification enabled, do not replace this with setInsecure()
//
// C++ raw string literal (R"(...)") preserves leading spaces - flush left
constexpr char ROOT_CA[] = R"(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----)";

void invalidate_token() {
    const std::lock_guard<std::mutex> lock(token_mutex);
    token = "";
    token_acquired_ms = 0;
    token_effective_lifetime_ms = 0;
}

String fetch_access_token(bool force_refresh = false) {
    {
        const std::lock_guard<std::mutex> lock(token_mutex);
        const unsigned long now_ms = millis();
        const bool has_token = token.length() > 0;
        const bool is_unexpired =
            has_token && (now_ms - token_acquired_ms < token_effective_lifetime_ms);

        if (!force_refresh && is_unexpired) {
            return token;
        }
    }

    WiFiClientSecure client;
    if (!Spotify::Detail::configure_tls(client)) {
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
    const unsigned long effective_ms = (lifetime_ms > REFRESH_MARGIN_MS)
                                           ? (lifetime_ms - REFRESH_MARGIN_MS)
                                           : (lifetime_ms / SHORT_LIVED_TOKEN_DIVISOR);

    const std::lock_guard<std::mutex> lock(token_mutex);
    token = new_token;
    token_acquired_ms = millis();
    token_effective_lifetime_ms = effective_ms;

    return token;
}

template <typename RequestFn> int execute_with_token_retry(RequestFn request) {
    String access_token = fetch_access_token();
    if (access_token.isEmpty()) {
        Serial.println("No access token available");
        return -1;
    }

    int http_code = request(access_token);

    if (http_code != HTTP_CODE_UNAUTHORIZED) {
        return http_code;
    }

    Serial.println("HTTP 401: refreshing token and retrying once");
    access_token = fetch_access_token(true);

    if (access_token.isEmpty()) {
        Serial.println("Token force-refresh returned empty token");
        return -1;
    }

    return request(access_token);
}

enum class PlayerCommand { Prev, Next, Play, Pause };

constexpr std::string_view cmd_to_endpoint(PlayerCommand cmd) {
    switch (cmd) {
    case PlayerCommand::Prev:
        return "previous";
    case PlayerCommand::Next:
        return "next";
    case PlayerCommand::Play:
        return "play";
    case PlayerCommand::Pause:
        return "pause";
    }

    return {};
}

constexpr std::string_view get_http_method_for(PlayerCommand cmd) {
    switch (cmd) {
    case PlayerCommand::Next:
    case PlayerCommand::Prev:
        return "POST";

    case PlayerCommand::Play:
    case PlayerCommand::Pause:
        return "PUT";
    }

    return "POST";
}

[[nodiscard]] bool send_player_cmd(PlayerCommand cmd) {
    const std::string request_url =
        std::string(SPOTIFY_PLAYER_ENDPOINT) + std::string(cmd_to_endpoint(cmd));

    const int http_code = execute_with_token_retry([&](const String &access_token) {
        WiFiClientSecure client;
        // if (!Detail::configure_tls(client)) {
        //     return -1;
        // }
        client.setInsecure();  // TEMPORARY

        HTTPClient http;
        if (!http.begin(client, request_url.c_str())) {
            return -1;
        }

        http.addHeader("Authorization", "Bearer " + access_token);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Content-Length", "0");

        const std::string_view method = get_http_method_for(cmd);

        const int http_code = (method == "PUT") ? http.PUT("") : http.POST("");
        if (http_code >= 400) {
            Serial.printf("Spotify error body: %s\n", http.getString().c_str());
        }
        http.end();
        return http_code;
    });

    switch (http_code) {
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
        Serial.printf("Unexpected HTTP response code: %d\n", http_code);
        return false;
    }
}

}  // namespace

namespace Spotify {

TrackInfo fetch_curr_track_info() {
    TrackInfo track_info;
    String payload;

    const int http_code = execute_with_token_retry([&](const String &access_token) {
        WiFiClientSecure client;
        if (!Detail::configure_tls(client)) {
            return -1;
        }

        Serial.println("Fetching playing track from Spotify...");

        HTTPClient http;
        if (!http.begin(client,
                "https://api.spotify.com/v1/me/player/"
                "currently-playing?additional_types=track,episode")) {
            Serial.println("Failed to begin HTTPS session");
            return -1;
        }

        http.addHeader("Authorization", "Bearer " + access_token);
        int http_code = http.GET();

        if (http_code == HTTP_CODE_OK) {
            payload = http.getString();
        } else if (http_code > 0) {
            Serial.printf("Spotify error (%d): %s\n", http_code, http.getString().c_str());
        } else {
            Serial.printf("HTTP %d: connection failed or blocked\n", http_code);
        }

        http.end();
        return http_code;
    });

    switch (http_code) {
    case (HTTP_CODE_OK): {
        Serial.printf("Received response from Spotify API: %s\n", payload.c_str());

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);

        if (err) {
            Serial.printf("Failed to parse JSON: %s\n", err.c_str());
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
        // Reached only if the initial request and the forced token refresh both returned
        // HTTP 401
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
    case -1:
        break;
    default:
        Serial.printf("Unexpected HTTP response code: %d\n", http_code);
        break;
    }

    return track_info;
}

void update_curr_track_info(TrackInfo &curr_track_info) {
    static unsigned long last_spotify_poll = 0;

    const unsigned long now_ms = millis();
    if (WiFi.status() == WL_CONNECTED && now_ms - last_spotify_poll >= SPOTIFY_POLL_INTERVAL_MS) {
        last_spotify_poll = now_ms;
        curr_track_info = fetch_curr_track_info();
    }
}

bool skip_next() { return send_player_cmd(PlayerCommand::Next); }
bool skip_prev() { return send_player_cmd(PlayerCommand::Prev); }

bool play_playback() { return send_player_cmd(PlayerCommand::Play); }
bool pause_playback() { return send_player_cmd(PlayerCommand::Pause); }

}  // namespace Spotify

namespace Spotify::Detail {

// Solves Unix Epoch default time problem by syncing the time with NTP,
// in order to validate TLS certificates for HTTPS requests (prevents 1970 clock default
// error)
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
        esp_sntp_setservername(0, NTP_SERVER);
        esp_sntp_init();
    } else {
        esp_sntp_restart();  // Already running - force a fresh request
    }

    const auto start = steady_clock::now();
    Serial.print("Waiting for NTP time sync: ");

    while (!is_ntp_synced.load(std::memory_order_acquire)) {
        if (steady_clock::now() - start >= NTP_SYNC_TIMEOUT_MS) {
            Serial.println();
            return false;
        }
        Serial.print('.');
        std::this_thread::sleep_for(500ms);
    }

    return true;
}

bool configure_tls(WiFiClientSecure &client) {
    // client.setCACert(ROOT_CA);
    client.setInsecure();

    client.setHandshakeTimeout(10);
    client.setTimeout(8);

    return true;
}

}  // namespace Spotify::Detail
