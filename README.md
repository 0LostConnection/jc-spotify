# JC3248W535EN — Spotify Connect remote

Landscape Spotify remote on the **JC3248W535EN** (ESP32-S3, 3.5″ AXS15231B QSPI + touch). Control playback on any Spotify Connect device: play/pause, skip, shuffle, like, device transfer, cover art, and on-device settings.

**Requires Spotify Premium** for Web API playback control.

## Features

- SoftAP WiFi captive portal (`JC-Spotify-Setup`)
- On-device Spotify OAuth **PKCE** (`https://jc-spotify.local/callback`)
- Now-playing UI (480×320) with cover art
- Transport, shuffle, like, device picker
- Settings: brightness, system info, factory reset (WiFi + Spotify)

## Board

| Piece | Detail |
|-------|--------|
| MCU | ESP32-S3 @ 240 MHz |
| Flash / PSRAM | 16 MB / 8 MB OPI |
| Panel | 320×480 IPS AXS15231B **QSPI** |
| Touch | AXS15231B **I2C** |
| Backlight | PWM GPIO 1 |
| TE | GPIO 38 |

## Quick start

1. Copy secrets and set your Spotify Client ID:

```bash
cp main/secrets.example.hpp main/secrets.hpp
# edit SPOTIFY_CLIENT_ID
```

2. In the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard), add redirect URI **exactly**:

```text
https://jc-spotify.local/callback
```

3. Build & flash:

```bash
export PATH="$HOME/.platformio/penv/bin:$PATH"
pio run -e JC3248W535EN
sg uucp -c 'pio run -e JC3248W535EN -t upload --upload-port /dev/ttyACM0'
```

4. First boot: join SoftAP `JC-Spotify-Setup` → save home WiFi → open `https://jc-spotify.local/` (accept cert warning) → authorize Spotify.

## Docs

| Doc | Purpose |
|-----|---------|
| [HANDOFF.md](HANDOFF.md) | Current status, pitfalls, next steps (agent / session handoff) |
| [docs/SPOTIFY_PLAN.md](docs/SPOTIFY_PLAN.md) | Product / architecture plan |
| [docs/hardware/](docs/hardware/) | Vendor schematics, pinout, datasheets |

## Layout

```text
main/
  main.cpp                 # display/touch → net task → player_loop
  secrets.example.hpp      # template → secrets.hpp (gitignored)
  board/                   # QSPI panel, TE, bounce flush, touch
  wifi/                    # SoftAP portal + STA + NVS
  spotify/                 # PKCE auth, Web API, cover art, tokens
  ui/                      # player, settings, status, icons, fonts
boards/                    # PlatformIO board def
docs/hardware/             # vendor PDFs / photos
partitions.csv             # 3 MB app partition
sdkconfig.defaults         # LVGL / WiFi / httpd knobs
```

## Stack

- PlatformIO + ESP-IDF 5.3.x (`framework = espidf`), C++17
- LVGL 9.3 via Component Manager (+ `esp_lvgl_port`, `esp_lcd_axs15231b`, `mdns`, `esp_jpeg`)

Default boot brightness: `board::display::kDefaultBrightness` in `main/board/display.hpp`.
