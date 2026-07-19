#pragma once

#include "esp_err.h"

#include <cstddef>

namespace wifi {

constexpr char kApSsid[] = "JC-Spotify-Setup";
constexpr char kHostname[] = "jc-spotify";

struct Creds {
    char ssid[33]{};
    char pass[65]{};
};

/** Load WiFi credentials from NVS. Returns ESP_ERR_NOT_FOUND if unset. */
esp_err_t load_creds(Creds *out);

/** Persist credentials to NVS. */
esp_err_t save_creds(const Creds &creds);

/** Erase stored credentials (forces portal on next boot). */
esp_err_t clear_creds();

/**
 * Connect using NVS credentials, or start SoftAP captive portal until the user
 * saves a network. Blocks until STA has an IP (or returns error).
 */
esp_err_t start();

bool is_connected();

/** Write STA IPv4 as dotted quad into out (min 16 bytes). */
esp_err_t get_ip_str(char *out, size_t out_len);

}  // namespace wifi
