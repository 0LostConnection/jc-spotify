#pragma once

#include "esp_err.h"

#include "spotify/tokens.hpp"

namespace spotify {

/**
 * Ensure a usable access token exists.
 * - Refreshes from NVS refresh_token when possible
 * - Otherwise runs on-device PKCE (mDNS + HTTP callback) until the user logs in
 */
esp_err_t ensure_authenticated(Tokens *out);

/** Refresh access token if expired / near expiry. Does not start PKCE. */
esp_err_t ensure_fresh_access(Tokens *inout);

/** Force interactive PKCE again (clears tokens). */
esp_err_t reauthenticate(Tokens *out);

}  // namespace spotify
