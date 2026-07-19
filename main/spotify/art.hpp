#pragma once

#include "esp_err.h"

#include <cstddef>
#include <cstdint>

namespace spotify {

/** Decoded cover art (RGB565, owned by art module until next successful load). */
struct ArtBitmap {
    const uint16_t *pixels = nullptr;
    uint16_t w = 0;
    uint16_t h = 0;
};

/**
 * Download JPEG from url (HTTPS, public CDN — no Bearer) and decode to RGB565 in PSRAM.
 * Scales so the longest side is <= ~160 px. On success, out points at an internal buffer
 * valid until the next call. On failure, out is cleared.
 */
esp_err_t art_load(const char *url, ArtBitmap *out);

/** Clear cached art URL / bitmap (e.g. nothing playing). */
void art_clear();

/** Last successfully loaded URL (empty if none). */
const char *art_current_url();

}  // namespace spotify
