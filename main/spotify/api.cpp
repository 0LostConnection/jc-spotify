#include "spotify/api.hpp"

#include "board/display.hpp"
#include "spotify/art.hpp"
#include "spotify/auth_pkce.hpp"
#include "spotify/tokens.hpp"
#include "ui/player_ui.hpp"
#include "ui/status_ui.hpp"
#include "wifi/provision.hpp"

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace spotify {
namespace {

constexpr char TAG[] = "spotify.api";
constexpr char kApiHost[] = "https://api.spotify.com";
constexpr int kPollMs = 2500;
constexpr size_t kRespMax = 12288;

/* Track last known shuffle/like for toggle commands without racing the UI. */
bool s_shuffle = false;
bool s_liked = false;
bool s_library_ok = true; /* false after 403 — scopes missing until re-auth */
bool s_need_library_reauth = false;
bool s_library_reauth_done = false; /* only auto re-auth once per boot */
char s_track_id[64]{};
char s_track_uri[96]{}; /* spotify:track:… Prefer official URI from playback JSON. */
char s_item_type[16]{};
/* Only re-query /library/contains when the track changes (or after backoff). */
char s_liked_uri[96]{};
TickType_t s_library_backoff_until = 0;
constexpr int kLibraryBackoffMs = 60000;

void copy_json_str(const cJSON *obj, const char *key, char *out, size_t out_len) {
    out[0] = '\0';
    const cJSON *item = cJSON_GetObjectItem(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        std::snprintf(out, out_len, "%s", item->valuestring);
    }
}

/** Build spotify:track:{id} (or keep a full URI if already provided). */
void make_track_uri(const char *id_or_uri, char *out, size_t out_len) {
    out[0] = '\0';
    if (!id_or_uri || !id_or_uri[0] || out_len < 16) {
        return;
    }
    if (std::strncmp(id_or_uri, "spotify:", 8) == 0) {
        std::snprintf(out, out_len, "%s", id_or_uri);
        return;
    }
    std::snprintf(out, out_len, "spotify:track:%s", id_or_uri);
}

/** Percent-encode a Spotify URI for query strings (':' → %3A). */
void uri_query_escape(const char *uri, char *out, size_t out_len) {
    size_t o = 0;
    for (size_t i = 0; uri && uri[i] && o + 4 < out_len; ++i) {
        const unsigned char c = static_cast<unsigned char>(uri[i]);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_' || c == '.' || c == '~') {
            out[o++] = static_cast<char>(c);
        } else {
            static const char *hex = "0123456789ABCDEF";
            out[o++] = '%';
            out[o++] = hex[(c >> 4) & 0xF];
            out[o++] = hex[c & 0xF];
        }
    }
    out[o] = '\0';
}

esp_http_client_method_t method_from_str(const char *method) {
    if (std::strcmp(method, "PUT") == 0) {
        return HTTP_METHOD_PUT;
    }
    if (std::strcmp(method, "POST") == 0) {
        return HTTP_METHOD_POST;
    }
    if (std::strcmp(method, "DELETE") == 0) {
        return HTTP_METHOD_DELETE;
    }
    return HTTP_METHOD_GET;
}

esp_err_t http_auth_request(Tokens *tokens, const char *method, const char *path, const char *body,
                            char *resp, size_t resp_len, int *status_out) {
    ESP_RETURN_ON_FALSE(tokens && tokens->access[0] && path, ESP_ERR_INVALID_ARG, TAG, "args");
    ESP_RETURN_ON_ERROR(ensure_fresh_access(tokens), TAG, "token");

    char url[320]{};
    std::snprintf(url, sizeof(url), "%s%s", kApiHost, path);

    char auth[560]{};
    std::snprintf(auth, sizeof(auth), "Bearer %s", tokens->access);

    const int body_len = body ? static_cast<int>(std::strlen(body)) : 0;

    esp_http_client_config_t cfg{};
    cfg.url = url;
    cfg.timeout_ms = 15000;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    ESP_RETURN_ON_FALSE(client, ESP_ERR_NO_MEM, TAG, "client");

    const auto http_method = method_from_str(method);
    esp_http_client_set_method(client, http_method);
    esp_http_client_set_header(client, "Authorization", auth);
    if (body_len > 0) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
    } else if (http_method == HTTP_METHOD_PUT || http_method == HTTP_METHOD_POST ||
               http_method == HTTP_METHOD_DELETE) {
        esp_http_client_set_header(client, "Content-Length", "0");
    }

    esp_err_t err = esp_http_client_open(client, body_len);
    int status = 0;
    int total = 0;
    if (err == ESP_OK) {
        if (body_len > 0) {
            const int written = esp_http_client_write(client, body, body_len);
            if (written < 0) {
                err = ESP_FAIL;
            }
        }
        if (err == ESP_OK) {
            esp_http_client_fetch_headers(client);
            status = esp_http_client_get_status_code(client);
            if (resp && resp_len > 0) {
                while (total + 1 < static_cast<int>(resp_len)) {
                    const int n = esp_http_client_read(client, resp + total,
                                                       static_cast<int>(resp_len) - 1 - total);
                    if (n < 0) {
                        err = ESP_FAIL;
                        break;
                    }
                    if (n == 0) {
                        break;
                    }
                    total += n;
                }
                resp[total] = '\0';
            }
        }
    }
    esp_http_client_cleanup(client);

    if (status_out) {
        *status_out = status;
    }
    if (err != ESP_OK) {
        return err;
    }

    if (status == 401) {
        ESP_LOGW(TAG, "%s %s → 401, refreshing", method, path);
        tokens->expires_at_ms = 0;
        ESP_RETURN_ON_ERROR(ensure_fresh_access(tokens), TAG, "reauth");
        return ESP_ERR_INVALID_STATE;
    }

    if (status == 204 || (status >= 200 && status < 300)) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "%s %s HTTP %d (%d bytes)", method, path, status, total);
    if (resp && total > 0) {
        ESP_LOGW(TAG, "body: %.200s", resp);
    }
    return ESP_FAIL;
}

esp_err_t http_auth_request_retry(Tokens *tokens, const char *method, const char *path,
                                  const char *body, char *resp, size_t resp_len, int *status_out) {
    esp_err_t err = http_auth_request(tokens, method, path, body, resp, resp_len, status_out);
    if (err == ESP_ERR_INVALID_STATE) {
        err = http_auth_request(tokens, method, path, body, resp, resp_len, status_out);
    }
    return err;
}

void parse_playback(const char *json, Playback *out) {
    *out = Playback{};
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return;
    }
    out->active = true;
    out->is_playing = cJSON_IsTrue(cJSON_GetObjectItem(root, "is_playing"));
    out->shuffle = cJSON_IsTrue(cJSON_GetObjectItem(root, "shuffle_state"));

    const cJSON *device = cJSON_GetObjectItem(root, "device");
    if (cJSON_IsObject(device)) {
        copy_json_str(device, "name", out->device, sizeof(out->device));
    }

    const cJSON *item = cJSON_GetObjectItem(root, "item");
    if (cJSON_IsObject(item)) {
        copy_json_str(item, "name", out->title, sizeof(out->title));
        copy_json_str(item, "id", out->track_id, sizeof(out->track_id));
        copy_json_str(item, "type", s_item_type, sizeof(s_item_type));
        /* Prefer item.uri from Spotify; fall back to spotify:{type}:{id}. */
        char uri_buf[96]{};
        copy_json_str(item, "uri", uri_buf, sizeof(uri_buf));
        if (uri_buf[0]) {
            std::snprintf(s_track_uri, sizeof(s_track_uri), "%s", uri_buf);
        } else if (out->track_id[0]) {
            make_track_uri(out->track_id, s_track_uri, sizeof(s_track_uri));
        } else {
            s_track_uri[0] = '\0';
        }
        const cJSON *artists = cJSON_GetObjectItem(item, "artists");
        if (cJSON_IsArray(artists) && cJSON_GetArraySize(artists) > 0) {
            const cJSON *a0 = cJSON_GetArrayItem(artists, 0);
            if (cJSON_IsObject(a0)) {
                copy_json_str(a0, "name", out->artist, sizeof(out->artist));
            }
        }

        /* Prefer album.images (tracks); fall back to item.images (episodes). */
        const cJSON *images = nullptr;
        const cJSON *album = cJSON_GetObjectItem(item, "album");
        if (cJSON_IsObject(album)) {
            images = cJSON_GetObjectItem(album, "images");
        }
        if (!cJSON_IsArray(images)) {
            images = cJSON_GetObjectItem(item, "images");
        }
        if (cJSON_IsArray(images)) {
            /* Pick closest to 300px (good quality, modest download). */
            int best_delta = 100000;
            const cJSON *best = nullptr;
            const int n = cJSON_GetArraySize(images);
            for (int i = 0; i < n; ++i) {
                const cJSON *im = cJSON_GetArrayItem(images, i);
                if (!cJSON_IsObject(im)) {
                    continue;
                }
                const cJSON *url = cJSON_GetObjectItem(im, "url");
                if (!cJSON_IsString(url) || !url->valuestring) {
                    continue;
                }
                const cJSON *height = cJSON_GetObjectItem(im, "height");
                const int h = cJSON_IsNumber(height) ? height->valueint : 0;
                const int delta = h > 0 ? (h > 300 ? h - 300 : 300 - h) : 9999;
                if (!best || delta < best_delta) {
                    best = im;
                    best_delta = delta;
                }
            }
            if (best) {
                copy_json_str(best, "url", out->art_url, sizeof(out->art_url));
            }
        }
    } else {
        s_item_type[0] = '\0';
        s_track_uri[0] = '\0';
        std::snprintf(out->title, sizeof(out->title), "%s", "Nothing playing");
        out->is_playing = false;
    }
    cJSON_Delete(root);
}

esp_err_t api_check_liked(Tokens *tokens, const char *track_uri, bool *liked_out) {
    ESP_RETURN_ON_FALSE(track_uri && track_uri[0] && liked_out, ESP_ERR_INVALID_ARG, TAG, "args");
    *liked_out = false;
    if (!s_library_ok) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (s_library_backoff_until != 0 && xTaskGetTickCount() < s_library_backoff_until) {
        return ESP_ERR_INVALID_STATE; /* still in 429 backoff */
    }

    /* Docs: GET /me/library/contains?uris=spotify%3Atrack%3A… (Feb 2026). */
    char uri_esc[160]{};
    uri_query_escape(track_uri, uri_esc, sizeof(uri_esc));
    char path[220]{};
    std::snprintf(path, sizeof(path), "/v1/me/library/contains?uris=%s", uri_esc);

    char resp[64]{};
    int status = 0;
    (void)http_auth_request_retry(tokens, "GET", path, nullptr, resp, sizeof(resp), &status);
    if (status == 403) {
        ESP_LOGW(TAG, "library API forbidden — re-auth to grant user-library scopes");
        s_library_ok = false;
        if (!s_library_reauth_done) {
            s_need_library_reauth = true;
        }
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (status == 429) {
        s_library_backoff_until = xTaskGetTickCount() + pdMS_TO_TICKS(kLibraryBackoffMs);
        ESP_LOGW(TAG, "library contains rate-limited — backoff %d ms", kLibraryBackoffMs);
        return ESP_ERR_INVALID_STATE;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "library contains HTTP %d body=%.80s", status, resp);
        return ESP_FAIL;
    }

    s_library_backoff_until = 0;
    cJSON *root = cJSON_Parse(resp);
    if (!root) {
        return ESP_FAIL;
    }
    if (cJSON_IsArray(root) && cJSON_GetArraySize(root) > 0) {
        *liked_out = cJSON_IsTrue(cJSON_GetArrayItem(root, 0));
    }
    cJSON_Delete(root);
    return ESP_OK;
}

}  // namespace

esp_err_t api_fetch_playback(Tokens *tokens, Playback *out) {
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null");
    *out = Playback{};

    char *resp = static_cast<char *>(heap_caps_malloc(kRespMax, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!resp) {
        resp = static_cast<char *>(std::malloc(kRespMax));
    }
    ESP_RETURN_ON_FALSE(resp, ESP_ERR_NO_MEM, TAG, "resp");

    int status = 0;
    esp_err_t err =
        http_auth_request_retry(tokens, "GET", "/v1/me/player", nullptr, resp, kRespMax, &status);

    if (err == ESP_OK && status == 204) {
        out->active = false;
        out->is_playing = false;
        std::snprintf(out->title, sizeof(out->title), "%s", "Nothing playing");
        std::snprintf(out->artist, sizeof(out->artist), "%s", "Start playback on a Spotify device");
        std::free(resp);
        s_track_id[0] = '\0';
        s_track_uri[0] = '\0';
        s_item_type[0] = '\0';
        s_liked_uri[0] = '\0';
        return ESP_OK;
    }

    if (err == ESP_OK && status >= 200 && status < 300) {
        parse_playback(resp, out);
        std::free(resp);
        std::snprintf(s_track_id, sizeof(s_track_id), "%s", out->track_id);
        s_shuffle = out->shuffle;
        if (s_track_uri[0] && std::strcmp(s_item_type, "track") == 0) {
            /* Cache: only hit /library/contains when the track URI changes. */
            if (std::strcmp(s_liked_uri, s_track_uri) == 0) {
                out->liked = s_liked;
            } else {
                bool liked = false;
                const esp_err_t lerr = api_check_liked(tokens, s_track_uri, &liked);
                if (lerr == ESP_OK) {
                    out->liked = liked;
                    s_liked = liked;
                    std::snprintf(s_liked_uri, sizeof(s_liked_uri), "%s", s_track_uri);
                } else {
                    /* Keep last known liked state if contains fails (don't wipe the heart). */
                    out->liked = s_liked;
                }
            }
        } else {
            out->liked = false;
            s_liked = false;
            s_liked_uri[0] = '\0';
        }
        return ESP_OK;
    }

    std::free(resp);
    return err == ESP_OK ? ESP_FAIL : err;
}

esp_err_t api_fetch_devices(Tokens *tokens, DeviceList *out) {
    ESP_RETURN_ON_FALSE(out, ESP_ERR_INVALID_ARG, TAG, "null");
    *out = DeviceList{};

    char *resp = static_cast<char *>(heap_caps_malloc(kRespMax, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!resp) {
        resp = static_cast<char *>(std::malloc(kRespMax));
    }
    ESP_RETURN_ON_FALSE(resp, ESP_ERR_NO_MEM, TAG, "resp");

    int status = 0;
    esp_err_t err = http_auth_request_retry(tokens, "GET", "/v1/me/player/devices", nullptr, resp,
                                            kRespMax, &status);
    if (err != ESP_OK || status < 200 || status >= 300) {
        std::free(resp);
        return err == ESP_OK ? ESP_FAIL : err;
    }

    cJSON *root = cJSON_Parse(resp);
    std::free(resp);
    if (!root) {
        return ESP_FAIL;
    }
    const cJSON *devices = cJSON_GetObjectItem(root, "devices");
    if (cJSON_IsArray(devices)) {
        const int n = cJSON_GetArraySize(devices);
        for (int i = 0; i < n && out->count < kMaxDevices; ++i) {
            const cJSON *d = cJSON_GetArrayItem(devices, i);
            if (!cJSON_IsObject(d)) {
                continue;
            }
            Device &item = out->items[out->count];
            copy_json_str(d, "id", item.id, sizeof(item.id));
            copy_json_str(d, "name", item.name, sizeof(item.name));
            copy_json_str(d, "type", item.type, sizeof(item.type));
            item.is_active = cJSON_IsTrue(cJSON_GetObjectItem(d, "is_active"));
            if (item.id[0] == '\0') {
                continue;
            }
            ++out->count;
        }
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "devices: %d", out->count);
    return ESP_OK;
}

esp_err_t api_transfer(Tokens *tokens, const char *device_id, bool play) {
    ESP_RETURN_ON_FALSE(device_id && device_id[0], ESP_ERR_INVALID_ARG, TAG, "device");

    /* Escape is unnecessary: Spotify device ids are alphanumeric. */
    char body[128]{};
    std::snprintf(body, sizeof(body), "{\"device_ids\":[\"%s\"],\"play\":%s}", device_id,
                  play ? "true" : "false");

    char resp[512]{};
    int status = 0;
    ESP_RETURN_ON_ERROR(
        http_auth_request_retry(tokens, "PUT", "/v1/me/player", body, resp, sizeof(resp), &status), TAG,
        "transfer");
    if (status == 404) {
        ESP_LOGW(TAG, "transfer: device not found");
        return ESP_ERR_NOT_FOUND;
    }
    return (status == 204 || (status >= 200 && status < 300)) ? ESP_OK : ESP_FAIL;
}

esp_err_t api_set_playing(Tokens *tokens, bool play) {
    const char *path = play ? "/v1/me/player/play" : "/v1/me/player/pause";
    char resp[512]{};
    int status = 0;
    ESP_RETURN_ON_ERROR(
        http_auth_request_retry(tokens, "PUT", path, nullptr, resp, sizeof(resp), &status), TAG,
        "play/pause");
    if (status == 404) {
        ESP_LOGW(TAG, "no active device for %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    return (status == 204 || (status >= 200 && status < 300)) ? ESP_OK : ESP_FAIL;
}

esp_err_t api_skip(Tokens *tokens, bool next) {
    const char *path = next ? "/v1/me/player/next" : "/v1/me/player/previous";
    char resp[512]{};
    int status = 0;
    ESP_RETURN_ON_ERROR(
        http_auth_request_retry(tokens, "POST", path, nullptr, resp, sizeof(resp), &status), TAG,
        "skip");
    if (status == 404) {
        return ESP_ERR_NOT_FOUND;
    }
    return (status == 204 || (status >= 200 && status < 300)) ? ESP_OK : ESP_FAIL;
}

esp_err_t api_set_shuffle(Tokens *tokens, bool enabled) {
    char path[64]{};
    std::snprintf(path, sizeof(path), "/v1/me/player/shuffle?state=%s", enabled ? "true" : "false");
    char resp[512]{};
    int status = 0;
    ESP_RETURN_ON_ERROR(
        http_auth_request_retry(tokens, "PUT", path, nullptr, resp, sizeof(resp), &status), TAG,
        "shuffle");
    if (status == 404) {
        return ESP_ERR_NOT_FOUND;
    }
    if (status == 204 || (status >= 200 && status < 300)) {
        s_shuffle = enabled;
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t api_set_liked(Tokens *tokens, const char *track_id, bool liked) {
    ESP_RETURN_ON_FALSE(track_id && track_id[0], ESP_ERR_INVALID_ARG, TAG, "track");
    if (!s_library_ok) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (std::strcmp(s_item_type, "track") != 0) {
        ESP_LOGW(TAG, "like only works for tracks (type=%s)", s_item_type);
        return ESP_ERR_NOT_SUPPORTED;
    }

    char uri[96]{};
    if (s_track_uri[0]) {
        std::snprintf(uri, sizeof(uri), "%s", s_track_uri);
    } else {
        make_track_uri(track_id, uri, sizeof(uri));
    }
    ESP_RETURN_ON_FALSE(uri[0], ESP_ERR_INVALID_ARG, TAG, "uri");

    /*
     * API reference: PUT/DELETE /me/library?uris=spotify%3Atrack%3A…
     * (JSON body returns 400 "Missing required field: uris" — Spotify wants the
     * query parameter, not a JSON body, despite migration-guide JS examples.)
     */
    char uri_esc[160]{};
    uri_query_escape(uri, uri_esc, sizeof(uri_esc));
    char path[220]{};
    std::snprintf(path, sizeof(path), "/v1/me/library?uris=%s", uri_esc);

    char resp[512]{};
    int status = 0;
    (void)http_auth_request_retry(tokens, liked ? "PUT" : "DELETE", path, nullptr, resp, sizeof(resp),
                                  &status);
    if (status == 403) {
        ESP_LOGW(TAG, "library API forbidden — re-auth to grant user-library scopes");
        s_library_ok = false;
        if (!s_library_reauth_done) {
            s_need_library_reauth = true;
        }
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (status == 204 || (status >= 200 && status < 300)) {
        s_liked = liked;
        if (uri[0]) {
            std::snprintf(s_liked_uri, sizeof(s_liked_uri), "%s", uri);
        }
        ESP_LOGI(TAG, "library %s %s ok", liked ? "save" : "remove", uri);
        return ESP_OK;
    }
    ESP_LOGW(TAG, "library %s HTTP %d body=%.200s", liked ? "save" : "remove", status, resp);
    return ESP_FAIL;
}

[[noreturn]] void player_loop(Tokens tokens) {
    ESP_LOGI(TAG, "player loop start");
    TickType_t last_poll = 0;

    for (;;) {
        if (s_need_library_reauth) {
            s_need_library_reauth = false;
            s_library_reauth_done = true;
            ESP_LOGW(TAG, "starting PKCE again for library scopes");
            board::display::lock(0);
            ui::create_status("Spotify re-auth",
                              "LIKE needs library permission.\n"
                              "Open https://jc-spotify.local/\n"
                              "Approve library access.");
            board::display::unlock();
            Tokens fresh{};
            if (reauthenticate(&fresh) == ESP_OK) {
                tokens = fresh;
                s_library_ok = true;
                board::display::lock(0);
                ui::create_player();
                board::display::unlock();
                last_poll = 0;
            } else {
                ESP_LOGE(TAG, "re-auth failed");
            }
        }

        const ui::PlayerCommand cmd = ui::take_player_command();
        bool refresh_soon = false;

        switch (cmd) {
        case ui::PlayerCommand::Play:
        case ui::PlayerCommand::Pause: {
            const bool want_play = cmd == ui::PlayerCommand::Play;
            const esp_err_t cerr = api_set_playing(&tokens, want_play);
            if (cerr == ESP_OK) {
                board::display::lock(0);
                ui::set_player_playing(want_play);
                board::display::unlock();
            } else {
                ESP_LOGW(TAG, "play/pause failed: %s", esp_err_to_name(cerr));
            }
            refresh_soon = true;
            break;
        }
        case ui::PlayerCommand::Prev:
        case ui::PlayerCommand::Next: {
            const esp_err_t cerr = api_skip(&tokens, cmd == ui::PlayerCommand::Next);
            if (cerr != ESP_OK) {
                ESP_LOGW(TAG, "skip failed: %s", esp_err_to_name(cerr));
            }
            refresh_soon = true;
            break;
        }
        case ui::PlayerCommand::ShuffleToggle: {
            const bool want = !s_shuffle;
            const esp_err_t cerr = api_set_shuffle(&tokens, want);
            if (cerr == ESP_OK) {
                board::display::lock(0);
                ui::set_player_shuffle(want);
                board::display::unlock();
            } else {
                ESP_LOGW(TAG, "shuffle failed: %s", esp_err_to_name(cerr));
            }
            refresh_soon = true;
            break;
        }
        case ui::PlayerCommand::LikeToggle: {
            if (s_track_id[0] == '\0') {
                ESP_LOGW(TAG, "like: no track id");
                break;
            }
            const bool want = !s_liked;
            board::display::lock(0);
            ui::set_player_liked(want); /* optimistic */
            board::display::unlock();
            const esp_err_t cerr = api_set_liked(&tokens, s_track_id, want);
            if (cerr == ESP_OK) {
                refresh_soon = true;
            } else {
                ESP_LOGW(TAG, "like failed: %s", esp_err_to_name(cerr));
                board::display::lock(0);
                ui::set_player_liked(!want); /* revert */
                board::display::unlock();
            }
            break;
        }
        case ui::PlayerCommand::OpenDevices: {
            DeviceList devices{};
            const esp_err_t derr = api_fetch_devices(&tokens, &devices);
            board::display::lock(0);
            if (derr == ESP_OK) {
                ui::show_device_picker(devices);
            } else {
                ESP_LOGW(TAG, "devices failed: %s", esp_err_to_name(derr));
                DeviceList empty{};
                ui::show_device_picker(empty);
            }
            board::display::unlock();
            break;
        }
        case ui::PlayerCommand::SelectDevice: {
            char device_id[64]{};
            if (!ui::take_selected_device_id(device_id, sizeof(device_id))) {
                ESP_LOGW(TAG, "transfer: no device id");
                break;
            }
            const esp_err_t terr = api_transfer(&tokens, device_id, true);
            board::display::lock(0);
            ui::hide_device_picker();
            board::display::unlock();
            if (terr != ESP_OK) {
                ESP_LOGW(TAG, "transfer failed: %s", esp_err_to_name(terr));
            }
            refresh_soon = true;
            break;
        }
        case ui::PlayerCommand::CloseDevices: {
            board::display::lock(0);
            ui::hide_device_picker();
            board::display::unlock();
            break;
        }
        case ui::PlayerCommand::FactoryReset: {
            ESP_LOGW(TAG, "factory reset: wiping WiFi + Spotify, then reboot");
            board::display::lock(0);
            ui::hide_config_popup();
            board::display::unlock();
            (void)wifi::clear_creds();
            (void)tokens_clear();
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
            break;
        }
        case ui::PlayerCommand::None:
        default:
            break;
        }

        if (refresh_soon) {
            last_poll = 0;
        }

        const TickType_t now = xTaskGetTickCount();
        if (last_poll == 0 || (now - last_poll) >= pdMS_TO_TICKS(kPollMs)) {
            last_poll = now;
            Playback pb{};
            const esp_err_t perr = api_fetch_playback(&tokens, &pb);
            if (perr == ESP_OK) {
                ui::PlayerView view{};
                view.is_playing = pb.is_playing;
                view.shuffle = pb.shuffle;
                view.liked = pb.liked;
                view.has_track = pb.active && pb.track_id[0] != '\0';
                std::snprintf(view.title, sizeof(view.title), "%s",
                              pb.title[0] ? pb.title : "Nothing playing");
                std::snprintf(view.artist, sizeof(view.artist), "%s", pb.artist);
                std::snprintf(view.device, sizeof(view.device), "%s",
                              pb.device[0] ? pb.device : "No device");
                board::display::lock(0);
                ui::update_player(view);
                board::display::unlock();

                if (pb.art_url[0]) {
                    if (std::strcmp(pb.art_url, art_current_url()) != 0) {
                        ArtBitmap art{};
                        if (art_load(pb.art_url, &art) == ESP_OK && art.pixels) {
                            board::display::lock(0);
                            ui::set_player_cover(art.pixels, art.w, art.h);
                            board::display::unlock();
                        } else {
                            ESP_LOGW(TAG, "art load failed");
                            board::display::lock(0);
                            ui::clear_player_cover();
                            board::display::unlock();
                        }
                    }
                } else if (art_current_url()[0]) {
                    art_clear();
                    board::display::lock(0);
                    ui::clear_player_cover();
                    board::display::unlock();
                }
            } else {
                ESP_LOGW(TAG, "poll failed: %s", esp_err_to_name(perr));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

}  // namespace spotify
