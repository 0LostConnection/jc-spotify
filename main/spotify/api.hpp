#pragma once

#include "esp_err.h"

#include "spotify/tokens.hpp"

namespace spotify {

struct Playback {
    bool active = false; /* true if Spotify returned a player payload */
    bool is_playing = false;
    bool shuffle = false;
    bool liked = false;
    char title[96]{};
    char artist[96]{};
    char device[48]{};
    char track_id[64]{};
    char art_url[256]{};
};

constexpr int kMaxDevices = 8;

struct Device {
    char id[64]{};
    char name[48]{};
    char type[24]{};
    bool is_active = false;
};

struct DeviceList {
    Device items[kMaxDevices]{};
    int count = 0;
};

/** GET /v1/me/player — ESP_OK with active=false on 204 (nothing playing). */
esp_err_t api_fetch_playback(Tokens *tokens, Playback *out);

/** GET /v1/me/player/devices */
esp_err_t api_fetch_devices(Tokens *tokens, DeviceList *out);

/** PUT /v1/me/player — transfer playback to device_id. */
esp_err_t api_transfer(Tokens *tokens, const char *device_id, bool play);

/** PUT play (play=true) or pause (play=false). */
esp_err_t api_set_playing(Tokens *tokens, bool play);

/** POST previous (next=false) or next (next=true). */
esp_err_t api_skip(Tokens *tokens, bool next);

/** PUT /v1/me/player/shuffle?state= */
esp_err_t api_set_shuffle(Tokens *tokens, bool enabled);

/** PUT/DELETE /v1/me/library?uris= */
esp_err_t api_set_liked(Tokens *tokens, const char *track_id, bool liked);

/**
 * Poll playback and handle UI transport commands forever.
 * Call after create_player(); uses display lock for UI updates.
 */
[[noreturn]] void player_loop(Tokens tokens);

}  // namespace spotify
