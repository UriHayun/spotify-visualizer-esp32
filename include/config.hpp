#pragma once

#include <chrono>
#include <cstddef>
#include <ctime>
#include <string_view>

namespace Config::Wifi {

constexpr std::size_t RECONN_INTERVAL_MS = 180;  // 3 minutes

}  // namespace Config::Wifi

namespace Config::Spotify {

constexpr unsigned long SPOTIFY_POLL_INTERVAL_MS = 15'000;

constexpr std::string_view NTP_SERVER = "pool.ntp.org";
constexpr std::chrono::milliseconds NTP_SYNC_TIMEOUT_MS = 20'000;
constexpr std::time_t MIN_VALID_EPOCH = 1'767'225'600;  // 01-01-2026 00:00:00 UTC

}  // namespace Config::Spotify

namespace Config::Serial {

// Speed at which data is sent over a serial communication cable (bps)
constexpr unsigned long MONITOR_BAUD_RATE_BPS = 115'200;

}  // namespace Config::Serial
