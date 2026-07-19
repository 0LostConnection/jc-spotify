#include "spotify/art.hpp"

#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "jpeg_decoder.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace spotify {
namespace {

constexpr char TAG[] = "spotify.art";
constexpr size_t kJpegMax = 96 * 1024;
constexpr size_t kRgbMax = 160 * 160 * 2; /* long side ~160 after scale */

char s_url[256]{};
uint8_t *s_jpeg = nullptr;
uint16_t *s_rgb = nullptr;
uint16_t s_w = 0;
uint16_t s_h = 0;

void *psram_or_malloc(size_t n) {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        p = std::malloc(n);
    }
    return p;
}

esp_err_t ensure_bufs() {
    if (!s_jpeg) {
        s_jpeg = static_cast<uint8_t *>(psram_or_malloc(kJpegMax));
        ESP_RETURN_ON_FALSE(s_jpeg, ESP_ERR_NO_MEM, TAG, "jpeg buf");
    }
    if (!s_rgb) {
        s_rgb = static_cast<uint16_t *>(psram_or_malloc(kRgbMax));
        ESP_RETURN_ON_FALSE(s_rgb, ESP_ERR_NO_MEM, TAG, "rgb buf");
    }
    return ESP_OK;
}

esp_err_t http_download(const char *url, uint8_t *buf, size_t buf_len, size_t *out_len) {
    ESP_RETURN_ON_FALSE(url && url[0] && buf && out_len, ESP_ERR_INVALID_ARG, TAG, "args");
    *out_len = 0;

    esp_http_client_config_t cfg{};
    cfg.url = url;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;
    cfg.max_redirection_count = 5;
    cfg.buffer_size = 2048;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    ESP_RETURN_ON_FALSE(client, ESP_ERR_NO_MEM, TAG, "client");

    esp_err_t err = esp_http_client_open(client, 0);
    int status = 0;
    size_t total = 0;
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            while (total + 1 < buf_len) {
                const int n = esp_http_client_read(client, reinterpret_cast<char *>(buf + total),
                                                   static_cast<int>(buf_len - total));
                if (n < 0) {
                    err = ESP_FAIL;
                    break;
                }
                if (n == 0) {
                    break;
                }
                total += static_cast<size_t>(n);
            }
            *out_len = total;
        } else {
            err = ESP_FAIL;
        }
    }
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status < 200 || status >= 300) {
        ESP_LOGW(TAG, "download HTTP %d (%u bytes)", status, static_cast<unsigned>(total));
        return ESP_FAIL;
    }
    if (total == 0) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "downloaded %u jpeg bytes", static_cast<unsigned>(total));
    return ESP_OK;
}

esp_jpeg_image_scale_t choose_scale(uint16_t w, uint16_t h) {
    const uint16_t side = w > h ? w : h;
    if (side >= 500) {
        return JPEG_IMAGE_SCALE_1_4; /* 640 → 160 */
    }
    if (side >= 200) {
        return JPEG_IMAGE_SCALE_1_2; /* 300 → 150 */
    }
    return JPEG_IMAGE_SCALE_0;
}

esp_err_t decode_jpeg(const uint8_t *jpeg, size_t jpeg_len, uint16_t *rgb, size_t rgb_bytes,
                      uint16_t *out_w, uint16_t *out_h) {
    esp_jpeg_image_cfg_t cfg{};
    cfg.indata = const_cast<uint8_t *>(jpeg);
    cfg.indata_size = static_cast<uint32_t>(jpeg_len);
    cfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
    cfg.flags.swap_color_bytes = 0; /* match LVGL LE RGB565 */

    esp_jpeg_image_output_t info{};
    ESP_RETURN_ON_ERROR(esp_jpeg_get_image_info(&cfg, &info), TAG, "info");
    cfg.out_scale = choose_scale(info.width, info.height);

    /* Re-query size after choosing scale. */
    ESP_RETURN_ON_ERROR(esp_jpeg_get_image_info(&cfg, &info), TAG, "info2");
    if (info.output_len == 0 || info.output_len > rgb_bytes) {
        ESP_LOGW(TAG, "decoded size %u too large (max %u), trying more scale",
                 static_cast<unsigned>(info.output_len), static_cast<unsigned>(rgb_bytes));
        if (cfg.out_scale < JPEG_IMAGE_SCALE_1_8) {
            cfg.out_scale = static_cast<esp_jpeg_image_scale_t>(cfg.out_scale + 1);
            ESP_RETURN_ON_ERROR(esp_jpeg_get_image_info(&cfg, &info), TAG, "info3");
        }
        if (info.output_len == 0 || info.output_len > rgb_bytes) {
            return ESP_ERR_NO_MEM;
        }
    }

    cfg.outbuf = reinterpret_cast<uint8_t *>(rgb);
    cfg.outbuf_size = static_cast<uint32_t>(rgb_bytes);
    ESP_RETURN_ON_ERROR(esp_jpeg_decode(&cfg, &info), TAG, "decode");
    *out_w = info.width;
    *out_h = info.height;
    ESP_LOGI(TAG, "decoded %ux%u RGB565", info.width, info.height);
    return ESP_OK;
}

}  // namespace

esp_err_t art_load(const char *url, ArtBitmap *out) {
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null");
    *out = ArtBitmap{};
    ESP_RETURN_ON_FALSE(url && url[0], ESP_ERR_INVALID_ARG, TAG, "url");

    if (std::strcmp(url, s_url) == 0 && s_rgb && s_w > 0 && s_h > 0) {
        out->pixels = s_rgb;
        out->w = s_w;
        out->h = s_h;
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ensure_bufs(), TAG, "bufs");

    size_t jpeg_len = 0;
    ESP_RETURN_ON_ERROR(http_download(url, s_jpeg, kJpegMax, &jpeg_len), TAG, "download");

    uint16_t w = 0;
    uint16_t h = 0;
    ESP_RETURN_ON_ERROR(decode_jpeg(s_jpeg, jpeg_len, s_rgb, kRgbMax, &w, &h), TAG, "decode");

    std::snprintf(s_url, sizeof(s_url), "%s", url);
    s_w = w;
    s_h = h;
    out->pixels = s_rgb;
    out->w = w;
    out->h = h;
    return ESP_OK;
}

void art_clear() {
    s_url[0] = '\0';
    s_w = 0;
    s_h = 0;
}

const char *art_current_url() {
    return s_url;
}

}  // namespace spotify
