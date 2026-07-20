# Handoff: JC3248W535EN Spotify Miniplayer

**Date:** 2026-07-20  
**Board:** JC3248W535EN (ESP32-S3, 320×480 AXS15231B QSPI + touch, 16MB flash, 8MB OPI PSRAM)  
**Repo:** `/home/geovane/Projects/jc-spotify`  
**Product goal:** Landscape Spotify Connect remote (play/pause, shuffle, like, devices, cover art, settings).  
**Plan doc:** [docs/SPOTIFY_PLAN.md](docs/SPOTIFY_PLAN.md)

---

## Status (stop point)

| Item | State |
|------|--------|
| Display / LVGL 9 / TE / bounce flush | Works |
| Landscape UI **480×320** (90° CW in flush) | Works |
| Touch (swap_xy + mirror for landscape) | Works with player UI |
| WiFi SoftAP captive portal | Works (`JC-Spotify-Setup` → `http://192.168.4.1`) |
| WiFi STA + NVS creds | Works (SSID `"."` is valid — do not block it) |
| STA runtime reconnect | **Fixed** — after first IP, reconnect forever (was capped at 8 tries) |
| Input sanitize (trim / CR-LF / length) | Done |
| Spotify PKCE + HTTPS callback | **Works end-to-end** |
| Spotify Client ID in `secrets.hpp` | Set (gitignored) |
| Now playing + transport UI | **Works** — Figma-based icon player |
| Device list + transfer | **Works** — tap Device line → modal → transfer |
| Settings | **Works** — brightness, system info, factory reset (WiFi+Spotify) |
| Cover art | **Works** — JPEG → RGB565 in **rounded** frame (static; no spin) |
| CJK / symbol fonts | **Works** — Source Han SC 16 fallback + `LV_SYMBOL_*` (RAM font copies) |
| Library contains rate limits | **Mitigated** — cache per track URI + 60s backoff on HTTP 429 |
| Transient TLS/DNS blips | **Mitigated** — one connect retry, 8s poll backoff; recovers on its own |
| Repo layout | Cleaned — no vendored `libraries/lvgl`, no StreamDeck leftovers |

**Current app behavior after flash:** display → `net` task → WiFi → auth (skip login UI if refresh token in NVS) → landscape player → poll `/me/player` ~2.5s (8s after failures) + cover art fetch + handle button / settings commands.

---

## UI (current)

Figma-driven layout in [`ui/player_ui.cpp`](main/ui/player_ui.cpp) + icon assets in [`ui/icons.*`](main/ui/icons.cpp) / [`ui/assets/`](main/ui/assets/):

```text
pad 20 all around; 20px gap between top block and bottom row
┌──────────────────────────────────────────────────────────┐
│  [banner 222×210 r12]   Title (wrap)                     │
│                         Artist                           │
│                         ⚙  (Settings — separate popup)    │
│                         Device: name ↓  (device picker)  │
│                                                          │
│  (20px gap)                                              │
│  prev  play/pause  next          ♥    🔀                 │
│  ←──────── 222px ────────→                               │
└──────────────────────────────────────────────────────────┘
```

- Accent green `0x1DB954` for liked / shuffle-on; prev/play/next flash green on press then white.  
- Text colors: title `0xE8EEF2`, artist `0x8AA0B0`, device `0x5A6A78`.  
- Gear opens **Settings** (not device picker). Device label alone opens devices.  
- Banner height is **210** so top + 20 gap + 50 controls + 40 pad fits 320.

### Settings

Order: **brightness → system info → reset**.

1. **Brightness** — LVGL slider (min 5%) → `board::display::set_brightness()`.  
   **Default / boot brightness:** `board::display::kDefaultBrightness` in [`main/board/display.hpp`](main/board/display.hpp) (currently **80**). Used by `backlight_on()`.  
2. **System info** — hostname `jc-spotify`, STA IP, MAC, free heap/PSRAM, app version.  
3. **Reset system** — confirm → `wifi::clear_creds()` + `spotify::tokens_clear()` + `esp_restart()` via `PlayerCommand::FactoryReset` on the `net` task.

### Fonts / blank boxes (important)

Montserrat 14/20 only cover **ASCII + FontAwesome symbols**. Unicode like `↓`, `—`, `●`, and CJK glyphs render as empty boxes.

**Do this instead:**

1. UI chrome: use `LV_SYMBOL_DOWN` / other `LV_SYMBOL_*`, and ASCII `-` / `*` — not fancy Unicode.  
2. Track metadata (CJK): `CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_16_CJK=y` + **RAM copies** of Montserrat with `.fallback` → Source Han (`ui/fonts.*`). **Never write `.fallback` on the const flash font objects** — that boot-loops on ESP32-S3 (`Cache disabled but cached memory region accessed`).  
3. After changing [`sdkconfig.defaults`](sdkconfig.defaults): `rm -f sdkconfig.JC3248W535EN` then rebuild.

---

## What works (board stack)

Keep these; they were painful to get right:

1. **`disp_on_off(false)`** → DISPON (`esp_lcd_axs15231b` inverts the bool).  
2. **PSRAM full-frame canvas + SRAM bounce strips** + RGB565 byte-swap in flush.  
3. **QSPI skips RASET** → strip flushes must advance `y` from 0 with `RAMWR` then `RAMWRC`.  
4. **Landscape = software remap in flush** (not MADCTL `swap_xy`) — matches vendor `LV_DISP_ROT_90`.  
5. **TE** wait once per frame (`y1 == 0`) + 50 ms timeout.  
6. **Partition:** custom [`partitions.csv`](partitions.csv) 3 MB app (cert bundle + WiFi + CJK font need it).  
7. **Networking off `app_main`:** `net` task **32 KB** stack — WiFi/TLS blew the default main stack (boot loop).

---

## Spotify auth (verified)

- **PKCE** (no client secret on device).  
- **Redirect URI (Dashboard) — exact match required:**  
  `https://jc-spotify.local/callback`  
  - Not `https://jc-spotify.local` (missing `/callback` → “redirect_uri: Not matching”).  
  - Spotify rejects plain `http://` except loopback `127.0.0.1`.  
- Device serves **HTTPS** via `esp_https_server` + self-signed cert in [`main/spotify/certs_pem.hpp`](main/spotify/certs_pem.hpp) (CN=`jc-spotify.local`).  
- **mDNS** hostname `jc-spotify` (`espressif/mdns`).  
- Browser must **accept cert warning** once, then open `https://jc-spotify.local/` (or `https://<device-ip>/`).  
- Tokens in NVS namespace `spotify` ([`tokens.cpp`](main/spotify/tokens.cpp)).  
- Authorize URL includes **`show_dialog=true`** so incremental scopes show consent again.

**Critical auth implementation details** ([`auth_pkce.cpp`](main/spotify/auth_pkce.cpp)):

1. **URI length:** `CONFIG_HTTPD_MAX_URI_LEN=2048` / `CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048` in [`sdkconfig.defaults`](sdkconfig.defaults). Spotify’s `/callback?code=…` exceeds 512 → “URI is too long”. After changing these: `rm -f sdkconfig.JC3248W535EN` then rebuild.  
2. **No nested TLS on httpd:** callback handler only captures the auth code and signals the `net` task. Token exchange to `accounts.spotify.com` runs **after** the HTTPS server is stopped (outbound TLS on the 32 KB `net` stack). Doing exchange inside the handler → stack overflow → `LoadProhibited` boot loop.  
3. HTTPS server `stack_size` bumped to **16 KB**; large redirect Location string kept in static storage.  
4. **`ensure_fresh_access()`** refreshes access token without starting PKCE (used by API worker).

**Scopes:**  
`user-read-playback-state` `user-modify-playback-state` `user-read-currently-playing` `user-library-read` `user-library-modify`

**Secrets:** copy [`main/secrets.example.hpp`](main/secrets.example.hpp) → `main/secrets.hpp` (gitignored), set `SPOTIFY_CLIENT_ID`.

---

## Player / Web API (verified)

[`spotify/api.cpp`](main/spotify/api.cpp) + [`ui/player_ui.cpp`](main/ui/player_ui.cpp):

- Poll `GET /v1/me/player` on the `net` task; UI updates under `board::display::lock`.  
- Default poll **2.5s**; after `ESP_ERR_HTTP_CONNECT` / poll failure use **8s** backoff, then return to 2.5s on success.  
- One extra retry on `ESP_ERR_HTTP_CONNECT` (each TLS attempt can burn the full timeout — do not stack many retries).  
- Commands from LVGL → volatile cmd id → worker (never do HTTPS on the LVGL task).  
- Controls: prev · play/pause · next · shuffle · like (icon buttons).  
- Library APIs (Feb 2026 migration — old `/me/tracks*` removed):
  - Save/remove: `PUT/DELETE /v1/me/library?uris=spotify%3Atrack%3A…` (query param required — JSON body returns **400 Missing required field: uris**)
  - Contains: `GET /v1/me/library/contains?uris=` — **only when track URI changes** (cached in `s_liked_uri`); **60s backoff** on HTTP 429
  - Prefer `item.uri` from playback JSON.  
- Like only for `item.type == "track"` (not podcasts).  
- Cover art from `album.images` / `item.images` (~300px JPEG) via [`spotify/art.*`](main/spotify/art.cpp) — **8s** download timeout; on failure **keep previous cover** (retry next poll).  
- **Premium required** for playback control.

**Library scopes:** older tokens without `user-library-*` get HTTP **403**. Inspect **status** even when `http_auth_request` returns `ESP_FAIL` (403 is not 2xx). Stop polling library APIs and run **one** PKCE re-auth (`show_dialog=true`).

**Transient network (expected):** TLS `-0x7280` (`MBEDTLS_ERR_SSL_CONN_EOF`), `getaddrinfo` **202** (`EAI_FAIL`), `Connection timed out before data was ready`, `select() timeout` → `ESP_ERR_HTTP_CONNECT`. Cert bundle is fine. Track changes are heavier (player + contains + art). UI may freeze briefly; polls resume when the radio/DNS recovers. Not a stuck task.

**WiFi STA** ([`wifi/provision.cpp`](main/wifi/provision.cpp)): `kStaMaxRetry` (8) applies only to **first join**. After `s_ever_got_ip`, always call `esp_wifi_connect()` on `STA_DISCONNECTED` (old code stopped forever after 8 drops → player looked dead until reboot).

---

## Reset paths

| Path | Effect |
|------|--------|
| **Settings → Reset system** | Clears WiFi NVS + Spotify tokens, reboots → SoftAP + PKCE again |
| `wifi::clear_creds()` | WiFi namespace only |
| `spotify::tokens_clear()` | Spotify logout only |
| esptool erase NVS | Full NVS wipe without reflashing app |

```bash
python ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 -p /dev/ttyACM0 erase_region 0x9000 0x6000
```

---

## Repo layout

```text
/
  README.md                 # user-facing overview
  HANDOFF.md                # this file (session / agent status)
  platformio.ini
  partitions.csv
  sdkconfig.defaults
  dependencies.lock         # IDF component pin (tracked)
  boards/320x480.json
  docs/
    README.md
    SPOTIFY_PLAN.md
    hardware/               # vendor PDFs / photos
  main/
    main.cpp                # display/touch then net task → player_loop
    secrets.example.hpp
    secrets.hpp             # gitignored
    idf_component.yml       # lvgl, esp_lvgl_port, axs15231b, mdns, esp_jpeg
    board/                  # panel, TE, bounce landscape flush, touch
    wifi/provision.*        # SoftAP portal + STA + NVS
    spotify/                # PKCE, API, art, tokens, certs_pem.hpp
    ui/                     # player + settings + status + icons + fonts
```

LVGL comes from **IDF Component Manager** (`managed_components/`, gitignored) — not a vendored `libraries/lvgl`.

---

## Flash / port notes

Device usually `/dev/ttyACM0` (`303a:1001`); after resets it may appear as `/dev/ttyACM1`. On this host the dialout group is used:

```bash
export PATH="$HOME/.platformio/penv/bin:$PATH"
cd /home/geovane/Projects/jc-spotify
sg dialout -c 'pio run -e JC3248W535EN -t upload --upload-port /dev/ttyACM0'
sg dialout -c 'pio device monitor --port /dev/ttyACM0'
```

Permanent: `sudo usermod -aG dialout "$USER"` then re-login.

Serial monitor from agents often fails (no TTY); use a short Python `serial` read (reopen on disconnect during boot loops) or a real terminal.

After `sdkconfig.defaults` changes: `rm -f sdkconfig.JC3248W535EN` then rebuild.

---

## Bugs already hit (do not repeat)

| Symptom | Cause | Fix |
|---------|--------|-----|
| White/blue columns | No bounce / wrong endian / full DMA from PSRAM | Vendor-style bounce + BE swap |
| Boot loop stack overflow | WiFi on default `main` stack | `net` task 32 KB + larger main stack |
| Boot loop rebooting forever | Bad STA + `esp_restart` into portal | Wipe creds → SoftAP **in-process** |
| Blocked SSID `"."` | Over-zealous validation | Only reject **empty after sanitize** |
| “Redirect URI is not secure” | `http://jc-spotify.local` | Use **`https://jc-spotify.local/callback`** |
| “redirect_uri: Not matching” | Dashboard missing `/callback` | Exact URI: `https://jc-spotify.local/callback` |
| “URI is too long” after authorize | `CONFIG_HTTPD_MAX_URI_LEN=512` | Raise to **2048** (+ req hdr); enlarge query/code buffers |
| Boot loop on HTTPS callback | Token exchange (TLS) inside httpd handler | Capture code in handler; exchange on **`net` task** after stopping HTTPS server |
| `/me/tracks/contains` 403 spam every poll | 403 returned as `ESP_FAIL` before status handled; library flag never set | Check **status==403** first → disable library calls + one re-auth; `show_dialog=true` |
| Blank boxes for `↓` / `—` / Chinese | Montserrat has no those glyphs | `LV_SYMBOL_*` + ASCII for chrome; Source Han CJK fallback for metadata |
| Boot loop: Cache disabled / write back to flash | Wrote `.fallback` onto `const` Montserrat in flash | Copy font structs to **RAM** (`ui/fonts.*`) then set fallback |
| Like button does nothing | Spotify **removed** `/me/tracks*`; JSON body to `/me/library` → 400 missing `uris` | Use **query** `PUT/DELETE /me/library?uris=` |
| `/me/library/contains` HTTP 429 every ~2.5s | Contains called on every poll for same track | Cache liked by track URI; 60s backoff on 429 |
| Player frozen until reboot after WiFi flap | STA reconnect stopped after 8 `DISCONNECTED` events | After first IP, reconnect forever (`s_ever_got_ip`) |
| Long freeze on track change then recover | TLS/DNS stall + 15s timeouts; art clear on fail | 8s art timeout; keep old cover; 8s poll backoff; one connect retry |

---

## Device picker (verified pattern)

- Tap **Device: … ↓** (label only — not the gear) → `OpenDevices` → `GET /v1/me/player/devices` on `net` task → modal on `lv_layer_top()`.  
- Tap a row → `SelectDevice` + buffered device id → `PUT /v1/me/player` JSON body `{"device_ids":["…"],"play":true}`.  
- HTTP helper supports optional JSON body (transfer only); other PUTs still send empty body.  
- Active device highlighted green with `*`; empty list shows “open Spotify on a speaker/phone”.

## Cover art (verified pattern)

- Parse best ~300px image URL from playback JSON (`album.images`, else `item.images`).  
- On URL change (still on `net` task): HTTPS GET (public CDN, no Bearer) → `esp_jpeg` TJpgDec → RGB565 in PSRAM.  
- Scale: 640→1/4, 300→1/2, smaller→1/1; long side capped ~160; UI scales into 222×210 rounded frame.  
- Download timeout **8s**; failed download does **not** clear the previous cover.  
- **No continuous rotation** — `full_refresh` + bounce remap makes LVGL transforms too expensive (UI lag).  
- Dep: `espressif/esp_jpeg` in [`main/idf_component.yml`](main/idf_component.yml).

## Suggested next session

1. Optional: defer cover art off the poll critical path (pending URL + independent retry) so metadata stays fresh during CDN stalls.  
2. Optional: persist brightness to NVS across reboot.  
3. Optional: offline / reconnecting UI state when polls fail.  
4. Optional: fuller CJK coverage if rare characters still box (custom font subset).  
5. Optional: PC helper if HTTPS + `.local` is painful on some phones.

If HTTPS + `.local` becomes painful on some phones: fallback is a PC helper with `http://127.0.0.1:PORT/callback` posting refresh token to the device.

---

## Environment

- PlatformIO: `~/.platformio/penv/bin/pio`  
- ESP-IDF 5.3.1, LVGL 9.3.0, esp_lvgl_port 2.6.3, axs15231b 2.1.0, mdns, esp_jpeg  
- Device MAC: `20:6e:f1:98:d4:88`  
- Home WiFi SSID: `Laboratório` (also tested with `"."`)  
- Last known STA IP: `192.168.0.100`  
- Serial: `sg dialout -c 'pio device monitor --port /dev/ttyACM0'`  
