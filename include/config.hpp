#pragma once

#include <chrono>
#include <cstddef>
#include <ctime>
#include <string_view>

namespace Config::Wifi {

constexpr std::size_t CONFIG_PORTAL_TIMEOUT_SEC = 180;  // 3 minutes

}  // namespace Config::Wifi

namespace Config::Spotify {

constexpr unsigned long SPOTIFY_POLL_INTERVAL_MS = 15'000;

constexpr std::string_view NTP_SERVER = "pool.ntp.org";
constexpr std::chrono::milliseconds NTP_SYNC_TIMEOUT_MS = std::chrono::milliseconds(20'000);
constexpr std::time_t MIN_VALID_EPOCH = 1'767'225'600;  // 01-01-2026 00:00:00 UTC

constexpr unsigned long REFRESH_MARGIN_MS = 5UL * 60UL * 1000UL;  // Refresh 5 minutes early
constexpr unsigned long SHORT_LIVED_TOKEN_DIVISOR = 2UL;

// PEM-encoded CA certificate trusted to validate api.spotify.com's TLS certificate chain
// Include the "-----BEGIN CERTIFICATE-----" and "-----END CERTIFICATE-----" lines
// Keep certificate verification enabled, do not replace this with setInsecure()
//
// C++ raw string literal (R"EOF(...)EOF") preserve leading spaces - flush left
constexpr char ROOT_CA[] = R"EOF(
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
-----END CERTIFICATE-----)EOF";

}  // namespace Config::Spotify

namespace Config::Display {

constexpr auto ALBUM_COVER_DOWNLOAD_TIMEOUT_MS = std::chrono::milliseconds{5000};

}  // namespace Config::Display

namespace Config::Serial_ {

// Speed at which data is sent over a serial communication cable (bps)
constexpr unsigned long MONITOR_BAUD_RATE_BPS = 115'200;

}  // namespace Config::Serial_
