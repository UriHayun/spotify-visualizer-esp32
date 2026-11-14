#pragma once

#include <cstddef>
#include <string_view>

namespace Config::Wifi {

constexpr std::size_t MAX_RECONN_ATTEMPTS = 20;

// WiFi reconnecting time-period interval (ms)
constexpr unsigned long RECONN_INTERVAL_MS = 5000;

}  // namespace Config::Wifi

namespace Config::Spotify {

constexpr unsigned long SPOTIFY_POLL_INTERVAL_MS = 15000;

constexpr std::string_view NTP_SERVER = "pool.ntp.org";
constexpr std::size_t NTP_SYNC_TIMEOUT_MS = 20000;
constexpr std::size_t MIN_VALID_EPOCH = 1'767'225'600;  // 01-01-2026 00:00:00 UTC

}  // namespace Config::Spotify

namespace Config::Serial {

// The speed at which data is sent over a serial communication cable (bps)
constexpr unsigned long MONITOR_BAUD_RATE_BPS = 115200;

}  // namespace Config::Serial
