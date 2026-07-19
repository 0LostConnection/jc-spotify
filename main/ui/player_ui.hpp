#pragma once

#include "spotify/api.hpp"

#include <cstddef>
#include <cstdint>

namespace ui {

enum class PlayerCommand {
    None,
    Play,
    Pause,
    Prev,
    Next,
    ShuffleToggle,
    LikeToggle,
    OpenDevices,
    SelectDevice,
    CloseDevices,
    FactoryReset,
};

struct PlayerView {
    char title[96]{};
    char artist[96]{};
    char device[48]{};
    bool is_playing = false;
    bool shuffle = false;
    bool liked = false;
    bool has_track = false;
};

/** Landscape 480×320 now-playing with transport, device picker, cover art. */
void create_player();

/** Update labels / buttons (call under display lock). */
void update_player(const PlayerView &view);

/** Optimistic play/pause label update (call under display lock). */
void set_player_playing(bool playing);

/** Optimistic shuffle / like (call under display lock). */
void set_player_shuffle(bool on);
void set_player_liked(bool on);

/**
 * Show RGB565 cover (pixels owned by caller / art module for lifetime of display).
 * Call under display lock. Static image in rounded frame (no spin — flush too costly).
 */
void set_player_cover(const uint16_t *rgb565, uint16_t w, uint16_t h);
void clear_player_cover();

/** Show / hide device transfer modal (call under display lock). */
void show_device_picker(const spotify::DeviceList &devices);
void hide_device_picker();

/** Hide settings popup (call under display lock). */
void hide_config_popup();

/**
 * After SelectDevice, copy the chosen Spotify device id into out.
 * Returns true if a non-empty id was pending.
 */
bool take_selected_device_id(char *out, size_t out_len);

/** Consume one pending command from the UI (thread-safe). */
PlayerCommand take_player_command();

}  // namespace ui
