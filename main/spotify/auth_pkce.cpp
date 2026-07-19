#include "spotify/auth_pkce.hpp"

#include "spotify/config.hpp"
#include "spotify/tokens.hpp"
#include "wifi/provision.hpp"

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"

#include "secrets.hpp"
#include "spotify/certs_pem.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace spotify {
namespace {

constexpr char TAG[] = "spotify.auth";
constexpr EventBits_t kAuthGotCode = BIT0;
constexpr EventBits_t kAuthFail = BIT1;

char s_verifier[128]{};
char s_challenge[128]{};
char s_auth_code[768]{};
char s_redirect_location[900]{};
httpd_handle_t s_httpd = nullptr;
EventGroupHandle_t s_events = nullptr;
Tokens s_pending{};

bool client_id_configured() {
    return std::strcmp(SPOTIFY_CLIENT_ID, "YOUR_SPOTIFY_CLIENT_ID") != 0 && SPOTIFY_CLIENT_ID[0] != '\0';
}

esp_err_t base64url_encode(const uint8_t *in, size_t in_len, char *out, size_t out_len) {
    size_t olen = 0;
    int ret = mbedtls_base64_encode(reinterpret_cast<unsigned char *>(out), out_len, &olen, in, in_len);
    if (ret != 0) {
        return ESP_FAIL;
    }
    for (size_t i = 0; i < olen; ++i) {
        if (out[i] == '+') {
            out[i] = '-';
        } else if (out[i] == '/') {
            out[i] = '_';
        }
    }
    while (olen > 0 && out[olen - 1] == '=') {
        out[--olen] = '\0';
    }
    out[olen] = '\0';
    return ESP_OK;
}

esp_err_t generate_pkce() {
    uint8_t raw[64];
    esp_fill_random(raw, sizeof(raw));
    ESP_RETURN_ON_ERROR(base64url_encode(raw, sizeof(raw), s_verifier, sizeof(s_verifier)), TAG, "verifier");

    uint8_t hash[32];
    mbedtls_sha256(reinterpret_cast<const unsigned char *>(s_verifier), std::strlen(s_verifier), hash, 0);
    ESP_RETURN_ON_ERROR(base64url_encode(hash, sizeof(hash), s_challenge, sizeof(s_challenge)), TAG, "challenge");
    return ESP_OK;
}

void url_encode(const char *in, char *out, size_t out_len) {
    static constexpr char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < out_len; ++i) {
        const unsigned char c = static_cast<unsigned char>(in[i]);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            out[o++] = static_cast<char>(c);
        } else if (c == ' ') {
            out[o++] = '%';
            out[o++] = '2';
            out[o++] = '0';
        } else {
            out[o++] = '%';
            out[o++] = hex[c >> 4];
            out[o++] = hex[c & 0x0f];
        }
    }
    out[o] = '\0';
}

esp_err_t http_body_collect(esp_http_client_handle_t client, char *buf, size_t buf_len, int *out_len) {
    int total = 0;
    while (total + 1 < static_cast<int>(buf_len)) {
        const int n = esp_http_client_read(client, buf + total, static_cast<int>(buf_len) - 1 - total);
        if (n < 0) {
            return ESP_FAIL;
        }
        if (n == 0) {
            break;
        }
        total += n;
    }
    buf[total] = '\0';
    if (out_len) {
        *out_len = total;
    }
    return ESP_OK;
}

esp_err_t parse_token_json(const char *json, Tokens *out) {
    cJSON *root = cJSON_Parse(json);
    ESP_RETURN_ON_FALSE(root, ESP_FAIL, TAG, "json");

    const cJSON *access = cJSON_GetObjectItem(root, "access_token");
    const cJSON *refresh = cJSON_GetObjectItem(root, "refresh_token");
    const cJSON *expires = cJSON_GetObjectItem(root, "expires_in");

    esp_err_t err = ESP_FAIL;
    if (cJSON_IsString(access) && access->valuestring) {
        std::snprintf(out->access, sizeof(out->access), "%s", access->valuestring);
        const int expires_in = cJSON_IsNumber(expires) ? expires->valueint : 3600;
        out->expires_at_ms = (esp_timer_get_time() / 1000) + static_cast<int64_t>(expires_in) * 1000;
        if (cJSON_IsString(refresh) && refresh->valuestring) {
            std::snprintf(out->refresh, sizeof(out->refresh), "%s", refresh->valuestring);
        }
        err = ESP_OK;
    }
    cJSON_Delete(root);
    return err;
}

esp_err_t token_request(const char *body, Tokens *out) {
    esp_http_client_config_t cfg{};
    cfg.url = kTokenUrl;
    cfg.method = HTTP_METHOD_POST;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    ESP_RETURN_ON_FALSE(client, ESP_ERR_NO_MEM, TAG, "client");

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, body, static_cast<int>(std::strlen(body)));

    esp_err_t err = esp_http_client_open(client, static_cast<int>(std::strlen(body)));
    if (err == ESP_OK) {
        esp_http_client_write(client, body, static_cast<int>(std::strlen(body)));
        esp_http_client_fetch_headers(client);
        char resp[1536]{};
        int resp_len = 0;
        err = http_body_collect(client, resp, sizeof(resp), &resp_len);
        const int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "token HTTP %d (%d bytes)", status, resp_len);
        if (err == ESP_OK && status >= 200 && status < 300) {
            err = parse_token_json(resp, out);
        } else {
            ESP_LOGW(TAG, "token body: %s", resp);
            err = ESP_FAIL;
        }
    }
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t exchange_code(const char *code, Tokens *out) {
    char redirect_enc[128]{};
    char code_enc[1024]{};
    url_encode(kRedirectUri, redirect_enc, sizeof(redirect_enc));
    url_encode(code, code_enc, sizeof(code_enc));

    char body[1536]{};
    std::snprintf(body, sizeof(body),
                  "grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&code_verifier=%s",
                  code_enc, redirect_enc, SPOTIFY_CLIENT_ID, s_verifier);
    Tokens got{};
    ESP_RETURN_ON_ERROR(token_request(body, &got), TAG, "exchange");
    ESP_RETURN_ON_FALSE(got.refresh[0], ESP_ERR_INVALID_RESPONSE, TAG, "no refresh");
    *out = got;
    return tokens_save(*out);
}

esp_err_t refresh_access(Tokens *inout) {
    ESP_RETURN_ON_FALSE(inout && inout->refresh[0], ESP_ERR_INVALID_STATE, TAG, "no refresh");
    char refresh_enc[700]{};
    url_encode(inout->refresh, refresh_enc, sizeof(refresh_enc));

    char body[900]{};
    std::snprintf(body, sizeof(body), "grant_type=refresh_token&refresh_token=%s&client_id=%s", refresh_enc,
                  SPOTIFY_CLIENT_ID);

    Tokens got = *inout;
    got.access[0] = '\0';
    ESP_RETURN_ON_ERROR(token_request(body, &got), TAG, "refresh");
    if (got.refresh[0] == '\0') {
        std::snprintf(got.refresh, sizeof(got.refresh), "%s", inout->refresh);
    }
    *inout = got;
    return tokens_save(*inout);
}

esp_err_t http_root(httpd_req_t *req) {
    /* Keep large URI off the httpd task stack (TLS already uses most of it). */
    char scopes_enc[256]{};
    char redirect_enc[128]{};
    url_encode(kScopes, scopes_enc, sizeof(scopes_enc));
    url_encode(kRedirectUri, redirect_enc, sizeof(redirect_enc));

    std::snprintf(s_redirect_location, sizeof(s_redirect_location),
                  "%s?client_id=%s&response_type=code&redirect_uri=%s&code_challenge_method=S256"
                  "&code_challenge=%s&scope=%s&show_dialog=true",
                  kAuthorizeUrl, SPOTIFY_CLIENT_ID, redirect_enc, s_challenge, scopes_enc);

    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", s_redirect_location);
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}

esp_err_t http_callback(httpd_req_t *req) {
    /* Do not call Spotify (outbound TLS) here — nested TLS on the httpd stack
     * overflows and panics. Capture the code; exchange on the net task. */
    char *query = static_cast<char *>(std::calloc(1, 1536));
    if (!query) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "oom");
        return ESP_FAIL;
    }

    esp_err_t err = httpd_req_get_url_query_str(req, query, 1536);
    if (err != ESP_OK) {
        std::free(query);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing query");
        return ESP_FAIL;
    }

    char errbuf[128]{};
    if (httpd_query_key_value(query, "error", errbuf, sizeof(errbuf)) == ESP_OK) {
        ESP_LOGW(TAG, "oauth error: %s", errbuf);
        std::free(query);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, errbuf);
        if (s_events) {
            xEventGroupSetBits(s_events, kAuthFail);
        }
        return ESP_FAIL;
    }
    if (httpd_query_key_value(query, "code", s_auth_code, sizeof(s_auth_code)) != ESP_OK ||
        s_auth_code[0] == '\0') {
        std::free(query);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "missing code");
        return ESP_FAIL;
    }
    std::free(query);

    constexpr char ok[] =
        "<!DOCTYPE html><html><body style=\"font-family:sans-serif;background:#101418;color:#e8eef2;"
        "padding:24px\"><h1>Almost done</h1><p>Finishing on the device — check the panel.</p>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, ok, HTTPD_RESP_USE_STRLEN);
    if (s_events) {
        xEventGroupSetBits(s_events, kAuthGotCode);
    }
    return ESP_OK;
}

esp_err_t start_mdns() {
    esp_err_t err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_RETURN_ON_ERROR(mdns_hostname_set(kMdnsHostname), TAG, "hostname");
    ESP_RETURN_ON_ERROR(mdns_instance_name_set("JC Spotify"), TAG, "instance");
    return ESP_OK;
}

esp_err_t start_auth_server() {
    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.servercert = reinterpret_cast<const uint8_t *>(kServerCertPem);
    conf.servercert_len = std::strlen(kServerCertPem) + 1;
    conf.prvtkey_pem = reinterpret_cast<const uint8_t *>(kServerKeyPem);
    conf.prvtkey_len = std::strlen(kServerKeyPem) + 1;
    conf.httpd.lru_purge_enable = true;
    conf.httpd.stack_size = 16384;

    ESP_RETURN_ON_ERROR(httpd_ssl_start(&s_httpd, &conf), TAG, "https start");

    const httpd_uri_t root = {.uri = "/", .method = HTTP_GET, .handler = http_root, .user_ctx = nullptr};
    const httpd_uri_t cb = {
        .uri = "/callback", .method = HTTP_GET, .handler = http_callback, .user_ctx = nullptr};
    httpd_register_uri_handler(s_httpd, &root);
    httpd_register_uri_handler(s_httpd, &cb);
    ESP_LOGI(TAG, "HTTPS auth server on https://%s.local/", kMdnsHostname);
    return ESP_OK;
}

void stop_auth_server() {
    if (s_httpd) {
        httpd_ssl_stop(s_httpd);
        s_httpd = nullptr;
    }
}

esp_err_t run_pkce_flow(Tokens *out) {
    ESP_RETURN_ON_FALSE(client_id_configured(), ESP_ERR_INVALID_STATE, TAG,
                        "Set SPOTIFY_CLIENT_ID in main/secrets.hpp");
    ESP_RETURN_ON_ERROR(generate_pkce(), TAG, "pkce");
    if (!s_events) {
        s_events = xEventGroupCreate();
        ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "events");
    }
    s_auth_code[0] = '\0';
    xEventGroupClearBits(s_events, kAuthGotCode | kAuthFail);

    ESP_RETURN_ON_ERROR(start_mdns(), TAG, "mdns");
    ESP_RETURN_ON_ERROR(start_auth_server(), TAG, "httpd");

    const EventBits_t bits =
        xEventGroupWaitBits(s_events, kAuthGotCode | kAuthFail, pdTRUE, pdFALSE, portMAX_DELAY);
    /* Stop server before outbound TLS so we are not nesting TLS on httpd. */
    stop_auth_server();
    mdns_free();

    if (bits & kAuthFail || s_auth_code[0] == '\0') {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "exchanging auth code on net task");
    ESP_RETURN_ON_ERROR(exchange_code(s_auth_code, &s_pending), TAG, "exchange");
    *out = s_pending;
    return ESP_OK;
}

}  // namespace

esp_err_t ensure_authenticated(Tokens *out) {
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null");
    ESP_RETURN_ON_FALSE(client_id_configured(), ESP_ERR_INVALID_STATE, TAG,
                        "Copy secrets.example.hpp → secrets.hpp and set Client ID");

    Tokens tokens{};
    if (tokens_load(&tokens) == ESP_OK && tokens.refresh[0]) {
        if (tokens_access_valid(tokens) || refresh_access(&tokens) == ESP_OK) {
            *out = tokens;
            ESP_LOGI(TAG, "authenticated via stored refresh token");
            return ESP_OK;
        }
        ESP_LOGW(TAG, "refresh failed — starting PKCE");
        tokens_clear();
    }

    return run_pkce_flow(out);
}

esp_err_t ensure_fresh_access(Tokens *inout) {
    ESP_RETURN_ON_FALSE(inout, ESP_ERR_INVALID_ARG, TAG, "null");
    if (tokens_access_valid(*inout)) {
        return ESP_OK;
    }
    if (inout->refresh[0] == '\0') {
        Tokens loaded{};
        ESP_RETURN_ON_ERROR(tokens_load(&loaded), TAG, "load");
        ESP_RETURN_ON_FALSE(loaded.refresh[0], ESP_ERR_INVALID_STATE, TAG, "no refresh");
        *inout = loaded;
        if (tokens_access_valid(*inout)) {
            return ESP_OK;
        }
    }
    return refresh_access(inout);
}

esp_err_t reauthenticate(Tokens *out) {
    tokens_clear();
    return run_pkce_flow(out);
}

}  // namespace spotify
