#include "board/display.hpp"
#include "board/touch.hpp"
#include "spotify/api.hpp"
#include "spotify/auth_pkce.hpp"
#include "spotify/tokens.hpp"
#include "ui/player_ui.hpp"
#include "ui/status_ui.hpp"
#include "wifi/provision.hpp"

#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "secrets.hpp"

#include <cstdio>
#include <cstring>

namespace {

constexpr char TAG[] = "app";
constexpr uint32_t kNetTaskStack = 32768;
constexpr UBaseType_t kNetTaskPrio = 5;

void log_boot_info() {
    esp_chip_info_t chip{};
    esp_chip_info(&chip);
    ESP_LOGI(TAG, "ESP32-S3 cores=%d rev=%d", chip.cores, chip.revision);
    ESP_LOGI(TAG, "free heap=%u free psram=%u",
             static_cast<unsigned>(esp_get_free_heap_size()),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
}

bool client_id_ok() {
    return std::strcmp(SPOTIFY_CLIENT_ID, "YOUR_SPOTIFY_CLIENT_ID") != 0 && SPOTIFY_CLIENT_ID[0] != '\0';
}

void status(const char *title, const char *detail) {
    board::display::lock(0);
    ui::create_status(title, detail);
    board::display::unlock();
}

void net_task(void *) {
    wifi::Creds creds{};
    const bool have_creds = wifi::load_creds(&creds) == ESP_OK && creds.ssid[0] != '\0';

    if (!have_creds) {
        status("WiFi setup",
               "1. Join WiFi: JC-Spotify-Setup\n"
               "2. Open http://192.168.4.1\n"
               "3. Enter home SSID + password");
    } else {
        board::display::lock(0);
        ui::set_status_detail("Connecting to saved network…");
        board::display::unlock();
    }

    /* Portal path never returns (reboots after save). STA returns when online. */
    ESP_ERROR_CHECK(wifi::start());

    if (!client_id_ok()) {
        status("Spotify Client ID",
               "Copy main/secrets.example.hpp → main/secrets.hpp\n"
               "Set SPOTIFY_CLIENT_ID from developer.spotify.com\n"
               "Redirect URI:\nhttps://jc-spotify.local/callback\n"
               "Then rebuild & flash.");
        ESP_LOGE(TAG, "SPOTIFY_CLIENT_ID not configured");
        vTaskDelete(nullptr);
        return;
    }

    char ip[16]{};
    if (wifi::get_ip_str(ip, sizeof(ip)) != ESP_OK) {
        std::snprintf(ip, sizeof(ip), "?.?.?.?");
    }

    spotify::Tokens tokens{};
    const bool have_token = spotify::tokens_load(&tokens) == ESP_OK && tokens.refresh[0] != '\0';
    if (!have_token) {
        char detail[192]{};
        std::snprintf(detail, sizeof(detail),
                      "On your phone/PC (same WiFi):\n"
                      "1. Open https://jc-spotify.local/\n"
                      "   (accept the cert warning)\n"
                      "   or https://%s/\n"
                      "2. Log into Spotify when asked.",
                      ip);
        status("Spotify login", detail);
    } else {
        status("Spotify", "Refreshing session…");
    }

    const esp_err_t auth_err = spotify::ensure_authenticated(&tokens);
    if (auth_err != ESP_OK) {
        status("Auth failed", "Check Client ID / redirect URI,\nthen reboot.");
        ESP_LOGE(TAG, "auth failed: %s", esp_err_to_name(auth_err));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "spotify auth ok — starting player");
    board::display::lock(0);
    ui::create_player();
    board::display::unlock();

    spotify::player_loop(tokens);
}

}  // namespace

extern "C" void app_main() {
    log_boot_info();

    board::display::Handles display{};
    ESP_ERROR_CHECK(board::display::start(&display));

    board::touch::Handles touch{};
    ESP_ERROR_CHECK(board::touch::start(display.lv_disp, &touch));

    status("JC Spotify", "Starting WiFi…");

    BaseType_t ok = xTaskCreatePinnedToCore(net_task, "net", kNetTaskStack, nullptr, kNetTaskPrio,
                                            nullptr, 0);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "failed to create net task");
        status("Error", "Could not start network task");
    }
}
