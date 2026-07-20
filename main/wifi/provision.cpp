#include "wifi/provision.hpp"

#include "esp_check.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <cctype>
#include <cstdio>
#include <cstring>

namespace wifi {
namespace {

constexpr char TAG[] = "wifi.prov";
constexpr char kNvsNs[] = "wifi";
constexpr char kNvsSsid[] = "ssid";
constexpr char kNvsPass[] = "pass";

constexpr EventBits_t kGotIp = BIT0;
constexpr EventBits_t kFail = BIT1;
constexpr int kStaMaxRetry = 8;
constexpr uint16_t kDnsPort = 53;
constexpr size_t kMaxSsidLen = 32; /* IEEE 802.11 + NUL elsewhere */
constexpr size_t kMaxPassLen = 64;

EventGroupHandle_t s_wifi_events = nullptr;
int s_retry = 0;
httpd_handle_t s_httpd = nullptr;
TaskHandle_t s_dns_task = nullptr;
volatile bool s_dns_run = false;
bool s_sta_connected = false;
bool s_ever_got_ip = false; /* after first IP, keep reconnecting forever */

/* Minimal captive-portal HTML (portrait-agnostic). */
constexpr char kIndexHtml[] =
    "<!DOCTYPE html><html><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>JC Spotify WiFi</title>"
    "<style>body{font-family:sans-serif;background:#101418;color:#e8eef2;margin:24px}"
    "input,button{font-size:18px;width:100%;margin:8px 0;padding:12px;border-radius:8px;border:0}"
    "button{background:#1db954;color:#000;font-weight:700}</style></head><body>"
    "<h1>JC Spotify</h1><p>Connect this panel to your WiFi.</p>"
    "<form method=POST action=/save>"
    "<input name=ssid placeholder=\"SSID\" required maxlength=32>"
    "<input name=pass type=password placeholder=\"Password\" maxlength=64>"
    "<button type=submit>Save &amp; Connect</button></form></body></html>";

constexpr char kSavedHtml[] =
    "<!DOCTYPE html><html><body style=\"font-family:sans-serif;background:#101418;color:#e8eef2;"
    "padding:24px\"><h1>Saved</h1><p>Rebooting to join the network…</p></body></html>";

esp_err_t nvs_init_once() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase");
        err = nvs_flash_init();
    }
    return err;
}

void url_decode_inplace(char *s) {
    char *r = s;
    char *w = s;
    while (*r) {
        if (*r == '+') {
            *w++ = ' ';
            ++r;
        } else if (*r == '%' && r[1] && r[2]) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f') {
                    return c - 'a' + 10;
                }
                if (c >= 'A' && c <= 'F') {
                    return c - 'A' + 10;
                }
                return -1;
            };
            const int hi = hex(r[1]);
            const int lo = hex(r[2]);
            if (hi >= 0 && lo >= 0) {
                *w++ = static_cast<char>((hi << 4) | lo);
                r += 3;
            } else {
                *w++ = *r++;
            }
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
}

/** Trim whitespace and drop CR/LF; optionally enforce max length. Empty after trim is allowed for passwords. */
void sanitize_field(char *s, size_t max_chars) {
    if (!s) {
        return;
    }
    char *start = s;
    while (*start && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    if (start != s) {
        std::memmove(s, start, std::strlen(start) + 1);
    }
    size_t len = std::strlen(s);
    while (len > 0 && std::isspace(static_cast<unsigned char>(s[len - 1]))) {
        s[--len] = '\0';
    }
    /* Strip embedded CR/LF from paste mishaps. */
    for (size_t i = 0; s[i];) {
        if (s[i] == '\r' || s[i] == '\n') {
            std::memmove(s + i, s + i + 1, std::strlen(s + i));
            continue;
        }
        ++i;
    }
    len = std::strlen(s);
    if (len > max_chars) {
        s[max_chars] = '\0';
    }
}

bool form_get(const char *body, const char *key, char *out, size_t out_len) {
    const size_t key_len = std::strlen(key);
    const char *p = body;
    while (p && *p) {
        if (std::strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            p += key_len + 1;
            size_t i = 0;
            while (*p && *p != '&' && i + 1 < out_len) {
                out[i++] = *p++;
            }
            out[i] = '\0';
            url_decode_inplace(out);
            return true;
        }
        p = std::strchr(p, '&');
        if (p) {
            ++p;
        }
    }
    return false;
}

esp_err_t http_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_save(httpd_req_t *req) {
    if (req->content_len <= 0 || req->content_len > 256) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad length");
        return ESP_FAIL;
    }
    char body[257]{};
    int got = httpd_req_recv(req, body, req->content_len);
    if (got <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv");
        return ESP_FAIL;
    }
    body[got] = '\0';

    Creds creds{};
    if (!form_get(body, "ssid", creds.ssid, sizeof(creds.ssid))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
    }
    form_get(body, "pass", creds.pass, sizeof(creds.pass));
    sanitize_field(creds.ssid, kMaxSsidLen);
    sanitize_field(creds.pass, kMaxPassLen);
    if (creds.ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "ssid required");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "portal saved ssid='%s' (len=%u)", creds.ssid,
             static_cast<unsigned>(std::strlen(creds.ssid)));
    ESP_ERROR_CHECK(save_creds(creds));
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, kSavedHtml, HTTPD_RESP_USE_STRLEN);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return ESP_OK;
}

esp_err_t http_captive(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

void dns_server_task(void *) {
    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "dns socket");
        vTaskDelete(nullptr);
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kDnsPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "dns bind");
        close(sock);
        vTaskDelete(nullptr);
        return;
    }

    uint8_t buf[512];
    while (s_dns_run) {
        sockaddr_in src{};
        socklen_t src_len = sizeof(src);
        const int len = recvfrom(sock, buf, sizeof(buf), 0, reinterpret_cast<sockaddr *>(&src), &src_len);
        if (len < 12) {
            continue;
        }
        /* Build a minimal DNS response pointing every A query at 192.168.4.1 */
        buf[2] = 0x81;
        buf[3] = 0x80;
        buf[6] = 0;
        buf[7] = 1; /* ANCOUNT = 1 */
        buf[8] = 0;
        buf[9] = 0;
        buf[10] = 0;
        buf[11] = 0;

        int resp_len = len;
        if (resp_len + 16 > static_cast<int>(sizeof(buf))) {
            continue;
        }
        buf[resp_len++] = 0xc0;
        buf[resp_len++] = 0x0c;
        buf[resp_len++] = 0x00;
        buf[resp_len++] = 0x01; /* A */
        buf[resp_len++] = 0x00;
        buf[resp_len++] = 0x01; /* IN */
        buf[resp_len++] = 0x00;
        buf[resp_len++] = 0x00;
        buf[resp_len++] = 0x00;
        buf[resp_len++] = 0x1e; /* TTL */
        buf[resp_len++] = 0x00;
        buf[resp_len++] = 0x04;
        buf[resp_len++] = 192;
        buf[resp_len++] = 168;
        buf[resp_len++] = 4;
        buf[resp_len++] = 1;
        sendto(sock, buf, resp_len, 0, reinterpret_cast<sockaddr *>(&src), src_len);
    }
    close(sock);
    vTaskDelete(nullptr);
}

esp_err_t start_httpd() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 8;
    ESP_RETURN_ON_ERROR(httpd_start(&s_httpd, &cfg), TAG, "httpd");

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = http_root, .user_ctx = nullptr};
    const httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = http_save, .user_ctx = nullptr};
    const httpd_uri_t generater = {
        .uri = "/generate_204", .method = HTTP_GET, .handler = http_captive, .user_ctx = nullptr};
    const httpd_uri_t hotspot = {
        .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = http_captive, .user_ctx = nullptr};
    const httpd_uri_t ncsi = {
        .uri = "/ncsi.txt", .method = HTTP_GET, .handler = http_captive, .user_ctx = nullptr};

    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &save);
    httpd_register_uri_handler(s_httpd, &generater);
    httpd_register_uri_handler(s_httpd, &hotspot);
    httpd_register_uri_handler(s_httpd, &ncsi);
    return ESP_OK;
}

void on_wifi(void *, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        ++s_retry;
        if (s_ever_got_ip) {
            /* Runtime drop: never give up — kStaMaxRetry is only for first join. */
            ESP_LOGW(TAG, "STA dropped — reconnecting (attempt %d)", s_retry);
            esp_wifi_connect();
        } else if (s_retry <= kStaMaxRetry) {
            ESP_LOGW(TAG, "STA retry %d/%d", s_retry, kStaMaxRetry);
            esp_wifi_connect();
        } else if (s_wifi_events) {
            ESP_LOGE(TAG, "STA join failed after %d tries", kStaMaxRetry);
            xEventGroupSetBits(s_wifi_events, kFail);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        auto *event = static_cast<ip_event_got_ip_t *>(data);
        ESP_LOGI(TAG, "got ip " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry = 0;
        s_sta_connected = true;
        s_ever_got_ip = true;
        if (s_wifi_events) {
            xEventGroupSetBits(s_wifi_events, kGotIp);
        }
    }
}

esp_err_t net_init() {
    ESP_RETURN_ON_ERROR(nvs_init_once(), TAG, "nvs");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop");
    if (!s_wifi_events) {
        s_wifi_events = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(s_wifi_events, ESP_ERR_NO_MEM, TAG, "events");
    }
    return ESP_OK;
}

esp_err_t start_sta(const Creds &creds) {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi, nullptr), TAG,
                        "wifi evt");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi, nullptr), TAG,
                        "ip evt");

    wifi_config_t wcfg{};
    std::strncpy(reinterpret_cast<char *>(wcfg.sta.ssid), creds.ssid, sizeof(wcfg.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char *>(wcfg.sta.password), creds.pass, sizeof(wcfg.sta.password) - 1);
    /* Open networks use empty password. */
    wcfg.sta.threshold.authmode = creds.pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wcfg), TAG, "cfg");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");

    const EventBits_t bits =
        xEventGroupWaitBits(s_wifi_events, kGotIp | kFail, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    if (bits & kGotIp) {
        return ESP_OK;
    }
    ESP_LOGW(TAG, "STA connect failed — clearing creds for portal");
    clear_creds();
    return ESP_FAIL;
}

esp_err_t start_portal() {
    ESP_LOGI(TAG, "starting SoftAP portal SSID=%s", kApSsid);
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init");

    wifi_config_t wcfg{};
    std::strncpy(reinterpret_cast<char *>(wcfg.ap.ssid), kApSsid, sizeof(wcfg.ap.ssid) - 1);
    wcfg.ap.ssid_len = static_cast<uint8_t>(std::strlen(kApSsid));
    wcfg.ap.channel = 1;
    wcfg.ap.max_connection = 4;
    wcfg.ap.authmode = WIFI_AUTH_OPEN;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), TAG, "mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP, &wcfg), TAG, "cfg");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start");
    ESP_RETURN_ON_ERROR(start_httpd(), TAG, "httpd");

    s_dns_run = true;
    xTaskCreate(dns_server_task, "dns_cap", 4096, nullptr, 5, &s_dns_task);

    /* Block forever — reboot happens from /save. */
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return ESP_OK;
}

}  // namespace

esp_err_t load_creds(Creds *out) {
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null");
    *out = Creds{};
    ESP_RETURN_ON_ERROR(nvs_init_once(), TAG, "nvs");
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(kNvsNs, NVS_READONLY, &h), TAG, "open");
    size_t ssid_len = sizeof(out->ssid);
    size_t pass_len = sizeof(out->pass);
    esp_err_t err = nvs_get_str(h, kNvsSsid, out->ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(h, kNvsPass, out->pass, &pass_len);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            out->pass[0] = '\0';
            err = ESP_OK;
        }
    }
    nvs_close(h);
    if (err == ESP_OK) {
        sanitize_field(out->ssid, kMaxSsidLen);
        sanitize_field(out->pass, kMaxPassLen);
        if (out->ssid[0] == '\0') {
            return ESP_ERR_NOT_FOUND;
        }
    }
    return err;
}

esp_err_t save_creds(const Creds &creds_in) {
    Creds creds = creds_in;
    sanitize_field(creds.ssid, kMaxSsidLen);
    sanitize_field(creds.pass, kMaxPassLen);
    ESP_RETURN_ON_FALSE(creds.ssid[0] != '\0', ESP_ERR_INVALID_ARG, TAG, "empty ssid");
    ESP_RETURN_ON_ERROR(nvs_init_once(), TAG, "nvs");
    nvs_handle_t h;
    ESP_RETURN_ON_ERROR(nvs_open(kNvsNs, NVS_READWRITE, &h), TAG, "open");
    esp_err_t err = nvs_set_str(h, kNvsSsid, creds.ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, kNvsPass, creds.pass);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t clear_creds() {
    ESP_RETURN_ON_ERROR(nvs_init_once(), TAG, "nvs");
    nvs_handle_t h;
    esp_err_t err = nvs_open(kNvsNs, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    nvs_erase_all(h);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

bool is_connected() {
    return s_sta_connected;
}

esp_err_t get_ip_str(char *out, size_t out_len) {
    ESP_RETURN_ON_FALSE(out && out_len >= 16, ESP_ERR_INVALID_ARG, TAG, "buf");
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    ESP_RETURN_ON_FALSE(netif, ESP_ERR_INVALID_STATE, TAG, "no sta netif");
    esp_netif_ip_info_t ip{};
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(netif, &ip), TAG, "ip info");
    std::snprintf(out, out_len, IPSTR, IP2STR(&ip.ip));
    return ESP_OK;
}

esp_err_t start() {
    ESP_RETURN_ON_ERROR(net_init(), TAG, "net");

    Creds creds{};
    const esp_err_t loaded = load_creds(&creds);
    if (loaded == ESP_OK && creds.ssid[0] != '\0') {
        ESP_LOGI(TAG, "connecting to '%s'", creds.ssid);
        if (start_sta(creds) == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "STA failed — wiping creds and starting portal");
        clear_creds();
        esp_wifi_stop();
        esp_wifi_deinit();
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi);
        if (s_wifi_events) {
            xEventGroupClearBits(s_wifi_events, kGotIp | kFail);
        }
        s_retry = 0;
        s_ever_got_ip = false;
        return start_portal();
    }

    return start_portal();
}

}  // namespace wifi
