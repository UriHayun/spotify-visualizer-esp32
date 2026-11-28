#pragma once

// ESP32 only supports 2.4GHz WiFi, not 5GHz. Make sure your router has the 2.4G band enabled
// WiFi password must be at least 8 characters long. If your router's password is too short, change
// the router password or use a different network Connecting to public WiFi (such as cafes, campus
// nteworks) may require web authentication (Portal), which the ESP32 cannot complete. Use an open
// or WPA2-encrypted network When the SSID contains Chinese or special characters, encoding issues
// may occur. It's recommended to use an SSID with only English letters and numbers

// Use char arrays because the Arduino Wi-Fi and TLS APIs expect NULL-terminated strings:
// C++'s std::string_view doesn't guarantee a trailing NULL character

namespace Secrets::Wifi {

// Make up one: connects to saved Wi-Fi or creates a password-protected setup AP ("AutoConnectAP"),
// recommended +8 characters
constexpr char AUTO_CONN_PASSWORD[] = "<your_auto_connection_password>";

}  // namespace Secrets::Wifi

namespace Secrets::Spotify {

// https://developer.spotify.com/dashboard
constexpr char CLIENT_ID[] = "<your_client_id";
constexpr char CLIENT_SECRET[] = "<your_client_secret";

// Spotify API OAuth Bearer Token
//
// - Open this URL in a browser (insert your: client ID, dashboard's rdirect URI):
//    https://accounts.spotify.com/authorize?client_id=<YOUR_CLIENT_ID>
//        &response_type=code
//        &redirect_uri=<YOUR_REDIRECT_URI>
//        &scope=user-read-currently-playing%20user-read-playback-state
//               %20user-modify-playback-state
// - The browser redirects to "<redirect_uri>?code=...". Copy the value after "code=", up to "&"
//
// - Run this command in your terminal (insert your: "code", redirect URI, client ID, client
//                                                                                    secret):
//    curl -X POST https://accounts.spotify.com/api/token \
//      -d grant_type=authorization_code \
//      -d "code=<YOUR_CODE>" \
//      -d "redirect_uri=<YOUR_REDIRECT_URI \
//      -d client_id=<YOUR_CLIENT_ID> \
//      -d client_secret=<YOUR_CLIENT_SECRET>
//
// - Paste "refresh_token" from the command above
constexpr char REFRESH_TOKEN[] = "<your_refresh_token";

}  // namespace Secrets::Spotify
