# Plan: Spotify Miniplayer (landscape)

**Board:** JC3248W535EN (ESP32-S3, 320×480 panel, AXS15231B QSPI + touch)  
**Orientation:** **Landscape** — logical UI **480×320** (`swap_xy` on panel + touch)  
**Auth:** On-device OAuth **PKCE** (no client secret)  
**WiFi:** Captive portal SoftAP on first boot  
**Reference:** [ThingPulse/esp32-spotify-remote](https://github.com/ThingPulse/esp32-spotify-remote) (behavior/UX inspiration; rewrite for ESP-IDF + LVGL 9)

**Requires Spotify Premium** for Web API playback control.

---

## Architecture

```text
Boot → NVS WiFi?
         ├─ no  → SoftAP "JC-Spotify-Setup" + captive portal → save SSID/pass → STA
         └─ yes → STA
              → NVS Spotify refresh token?
                   ├─ no  → mDNS jc-spotify.local + HTTP /callback PKCE → save tokens
                   └─ yes → refresh access token
                        → poll /me/player + LVGL player UI
                        → control play/pause/shuffle/like/devices
```

---

## Features (v1)

| Control | API |
|---------|-----|
| Play / pause | `PUT /v1/me/player/play` · `.../pause` |
| Prev / next | `POST .../previous` · `.../next` |
| Shuffle | `PUT .../shuffle?state=` |
| Favorite / unfavorite | `PUT/DELETE /v1/me/library?uris=` (Feb 2026; old `/me/tracks*` removed) |
| Device list + transfer | `GET .../devices` · `PUT /v1/me/player` |
| Now playing + art URL | `GET /v1/me/player` |
| Cover art | HTTPS JPEG → RGB565 → LVGL (vinyl-style rotate) |

**OAuth scopes:** `user-read-playback-state` `user-modify-playback-state` `user-read-currently-playing` `user-library-read` `user-library-modify`

**Redirect URI (Spotify Dashboard):** `https://jc-spotify.local/callback`  
(Spotify rejects plain `http://` except `127.0.0.1`. Device serves HTTPS with a self-signed cert — accept the browser warning once.)

---

## Landscape UI (480×320)

```text
┌────────────────────────────────────────────────────────────┐
│  [vinyl/cover]   Title                                     │
│                  Artist                                    │
│                  Device: Echo Dot ▾                        │
│                                                            │
│         ⏮    ▶/❚❚    ⏭      🔀    ♥                       │
└────────────────────────────────────────────────────────────┘
```

Device tap opens a modal list (PC, Firefox, Echo Dot, …).

---

## Module layout

```text
main/
  secrets.example.hpp      # SPOTIFY_CLIENT_ID template
  secrets.hpp              # gitignored
  wifi/provision.*         # SoftAP portal + STA + NVS creds
  spotify/auth_pkce.*      # OAuth PKCE + NVS tokens
  spotify/api.*            # Web API client (worker task)
  spotify/art.*            # JPEG download / decode
  ui/player_ui.*           # LVGL landscape miniplayer
  board/*                  # keep QSPI/TE/bounce; enable swap_xy
```

---

## Build order

1. **WiFi captive portal + STA** + landscape display/touch  
2. **OAuth PKCE** + token NVS + status UI  
3. **Now playing poll** + play/pause  
4. Shuffle + favorite  
5. Device picker  
6. Cover art + vinyl animation  
7. Polish (errors, re-auth, forget WiFi)

---

## Critical board notes (unchanged)

- `disp_on_off(false)` → DISPON (axs15231b quirk)  
- PSRAM canvas + SRAM bounce flush + RGB565 byte-swap  
- Full refresh (QSPI skips RASET)  
- TE sync on frame start  

---

## Out of scope (v1)

- On-device audio playback  
- Playlist browse / search  
- Clock / diagnostics views from ThingPulse  
