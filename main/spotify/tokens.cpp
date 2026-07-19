#include "spotify/tokens.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cstdio>
#include <cstring>

namespace spotify {
namespace {

constexpr char TAG[] = "spotify.tokens";
constexpr char kNs[] = "spotify";
constexpr char kAccess[] = "access";
constexpr char kRefresh[] = "refresh";
constexpr char kExpiry[] = "expiry";

esp_err_t nvs_ready() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase");
        err = nvs_flash_init();
    }
    return err;
}

}  // namespace

esp_err_t tokens_load(Tokens *out) {
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null");
    *out = Tokens{};
    ESP_RETURN_ON_ERROR(nvs_ready(), TAG, "nvs");
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(kNs, NVS_READONLY, &h), TAG, "open");

    size_t access_len = sizeof(out->access);
    size_t refresh_len = sizeof(out->refresh);
    esp_err_t err = nvs_get_str(h, kRefresh, out->refresh, &refresh_len);
    if (err == ESP_OK) {
        err = nvs_get_str(h, kAccess, out->access, &access_len);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            out->access[0] = '\0';
            err = ESP_OK;
        }
    }
    if (err == ESP_OK) {
        uint64_t expiry = 0;
        if (nvs_get_u64(h, kExpiry, &expiry) == ESP_OK) {
            out->expires_at_ms = static_cast<int64_t>(expiry);
        }
    }
    nvs_close(h);
    return err;
}

esp_err_t tokens_save(const Tokens &tokens) {
    ESP_RETURN_ON_ERROR(nvs_ready(), TAG, "nvs");
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(kNs, NVS_READWRITE, &h), TAG, "open");
    esp_err_t err = nvs_set_str(h, kRefresh, tokens.refresh);
    if (err == ESP_OK) {
        err = nvs_set_str(h, kAccess, tokens.access);
    }
    if (err == ESP_OK) {
        err = nvs_set_u64(h, kExpiry, static_cast<uint64_t>(tokens.expires_at_ms));
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t tokens_clear() {
    ESP_RETURN_ON_ERROR(nvs_ready(), TAG, "nvs");
    nvs_handle_t h;
    esp_err_t err = nvs_open(kNs, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_all(h);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool tokens_access_valid(const Tokens &tokens, int skew_ms) {
    if (tokens.access[0] == '\0') {
        return false;
    }
    const int64_t now = esp_timer_get_time() / 1000;
    return tokens.expires_at_ms - skew_ms > now;
}

}  // namespace spotify
