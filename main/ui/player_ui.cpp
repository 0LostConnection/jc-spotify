#include "ui/player_ui.hpp"

#include "board/display.hpp"
#include "ui/fonts.hpp"
#include "ui/icons.hpp"
#include "wifi/provision.hpp"

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "lvgl.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ui {
namespace {

lv_obj_t *s_scr = nullptr;
lv_obj_t *s_title = nullptr;
lv_obj_t *s_artist = nullptr;
lv_obj_t *s_device_btn = nullptr;
lv_obj_t *s_device_lbl = nullptr;
lv_obj_t *s_play_btn = nullptr;
lv_obj_t *s_play_img = nullptr;
lv_obj_t *s_prev_img = nullptr;
lv_obj_t *s_next_img = nullptr;
lv_obj_t *s_shuf_btn = nullptr;
lv_obj_t *s_shuf_img = nullptr;
lv_obj_t *s_like_btn = nullptr;
lv_obj_t *s_like_img = nullptr;
lv_obj_t *s_picker = nullptr;
lv_obj_t *s_config = nullptr;
lv_obj_t *s_art_frame = nullptr;
lv_obj_t *s_art_img = nullptr;
lv_obj_t *s_art_lbl = nullptr;

lv_image_dsc_t s_art_dsc{};

bool s_playing = false;
bool s_shuffle = false;
bool s_liked = false;
volatile int s_cmd = 0;
char s_selected_device_id[64]{};

constexpr uint32_t kGreen = 0x1DB954;
constexpr uint32_t kMuted = 0x2A3540;
constexpr uint32_t kTextDark = 0x101418;
constexpr uint32_t kTextLight = 0xE8EEF2;
constexpr uint32_t kTextMuted = 0x8AA0B0;
constexpr uint32_t kTextDim = 0x5A6A78;
constexpr uint32_t kPanel = 0x1A2228;
constexpr uint32_t kIconIdle = 0xFFFFFF;
constexpr uint32_t kBg = 0x000000;
constexpr uint32_t kArtPlaceholder = 0xD9D9D9;
constexpr int kPad = 20;
constexpr int kArtW = 222;
/* 320 − 2×pad − gap − controls = 210 so top/bottom sit 20px apart. */
constexpr int kArtH = 210;
constexpr int kControlsH = 50;

/* Command ids: 1=Play 2=Pause 3=Prev 4=Next 5=Shuf 6=Like 7=OpenDevices 8=Close 9=FactoryReset */
constexpr int kCmdOpenDevices = 7;
constexpr int kCmdCloseDevices = 8;
constexpr int kCmdFactoryReset = 9;
constexpr int kCmdSelectBase = 10;
constexpr int kBrightnessMin = 5;

spotify::DeviceList s_picker_devices{};

void style_btn(lv_obj_t *btn, uint32_t bg) {
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 4, 0);
}

void set_icon_color(lv_obj_t *img, uint32_t color) {
    if (!img) {
        return;
    }
    lv_obj_set_style_image_recolor(img, lv_color_hex(color), 0);
    lv_obj_set_style_image_recolor_opa(img, LV_OPA_COVER, 0);
}

void on_cmd_clicked(lv_event_t *e) {
    s_cmd = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
}

void on_play_clicked(lv_event_t *e) {
    (void)e;
    s_cmd = s_playing ? 2 : 1;
}

void on_device_row_clicked(lv_event_t *e) {
    const int idx = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
    if (idx < 0 || idx >= s_picker_devices.count) {
        return;
    }
    std::snprintf(s_selected_device_id, sizeof(s_selected_device_id), "%s",
                  s_picker_devices.items[idx].id);
    s_cmd = kCmdSelectBase;
}

void on_icon_press(lv_event_t *e) {
    auto *img = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    set_icon_color(img, kGreen);
}

void on_icon_release(lv_event_t *e) {
    auto *img = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
    set_icon_color(img, kIconIdle);
}

void attach_press_flash(lv_obj_t *btn, lv_obj_t *img) {
    lv_obj_add_event_cb(btn, on_icon_press, LV_EVENT_PRESSED, img);
    lv_obj_add_event_cb(btn, on_icon_release, LV_EVENT_RELEASED, img);
    lv_obj_add_event_cb(btn, on_icon_release, LV_EVENT_PRESS_LOST, img);
}

lv_obj_t *make_icon_btn(lv_obj_t *parent, const lv_image_dsc_t *dsc, int w, int h, lv_obj_t **out_img) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *img = lv_image_create(btn);
    lv_image_set_src(img, dsc);
    lv_obj_set_size(img, dsc->header.w, dsc->header.h);
    set_icon_color(img, kIconIdle);
    lv_obj_center(img);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(img, LV_OBJ_FLAG_EVENT_BUBBLE);
    if (out_img) {
        *out_img = img;
    }
    return btn;
}

void sync_play_icon() {
    if (!s_play_img) {
        return;
    }
    lv_image_set_src(s_play_img, s_playing ? &ui_img_pause : &ui_img_play);
    set_icon_color(s_play_img, kIconIdle);
}

void sync_toggle_btns() {
    if (s_shuf_img) {
        set_icon_color(s_shuf_img, s_shuffle ? kGreen : kIconIdle);
    }
    if (s_like_img) {
        set_icon_color(s_like_img, s_liked ? kGreen : kIconIdle);
    }
}

void destroy_picker() {
    if (s_picker) {
        lv_obj_delete(s_picker);
        s_picker = nullptr;
    }
}

void destroy_config() {
    if (s_config) {
        lv_obj_delete(s_config);
        s_config = nullptr;
    }
}

void on_brightness_changed(lv_event_t *e) {
    auto *slider = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int pct = static_cast<int>(lv_slider_get_value(slider));
    if (pct < kBrightnessMin) {
        pct = kBrightnessMin;
        lv_slider_set_value(slider, pct, LV_ANIM_OFF);
    }
    board::display::set_brightness(pct);
    if (auto *lbl = static_cast<lv_obj_t *>(lv_event_get_user_data(e))) {
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%d%%", pct);
        lv_label_set_text(lbl, buf);
    }
}

void fill_system_info(char *out, size_t out_len) {
    char ip[16] = "-";
    (void)wifi::get_ip_str(ip, sizeof(ip));

    uint8_t mac[6]{};
    char mac_str[18] = "-";
    if (esp_wifi_get_mac(WIFI_IF_STA, mac) == ESP_OK) {
        std::snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1],
                      mac[2], mac[3], mac[4], mac[5]);
    }

    const esp_app_desc_t *app = esp_app_get_description();
    const char *ver = (app && app->version[0]) ? app->version : "-";

    const size_t heap_free = esp_get_free_heap_size();
    const size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    std::snprintf(out, out_len,
                  "Host  %s\n"
                  "IP    %s\n"
                  "MAC   %s\n"
                  "Heap  %u KB free\n"
                  "PSRAM %u KB free\n"
                  "App   %s",
                  wifi::kHostname, ip, mac_str, static_cast<unsigned>(heap_free / 1024U),
                  static_cast<unsigned>(psram_free / 1024U), ver);
}

void show_reset_confirm() {
    if (!s_config) {
        return;
    }

    lv_obj_t *overlay = lv_obj_create(s_config);
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(
        overlay, [](lv_event_t *e) { lv_obj_delete(static_cast<lv_obj_t *>(lv_event_get_target(e))); },
        LV_EVENT_CLICKED, nullptr);

    lv_obj_t *box = lv_obj_create(overlay);
    lv_obj_set_size(box, 300, 150);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(kPanel), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(kMuted), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 12, 0);
    lv_obj_set_style_pad_all(box, 12, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 10, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(
        box, [](lv_event_t *e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Reset system?");
    lv_obj_set_style_text_color(title, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(title, font_20(), 0);

    lv_obj_t *msg = lv_label_create(box);
    lv_label_set_text(msg, "Clears WiFi and Spotify login,\nthen reboots.");
    lv_obj_set_style_text_color(msg, lv_color_hex(kTextMuted), 0);
    lv_obj_set_style_text_font(msg, font_14(), 0);
    lv_obj_set_width(msg, 276);

    lv_obj_t *row = lv_obj_create(box);
    lv_obj_set_size(row, 276, 36);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cancel = lv_button_create(row);
    lv_obj_set_size(cancel, 120, 32);
    style_btn(cancel, kMuted);
    lv_obj_align(cancel, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(
        cancel,
        [](lv_event_t *e) {
            auto *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
            lv_obj_t *ov = lv_obj_get_parent(lv_obj_get_parent(btn));
            lv_obj_delete(ov);
        },
        LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cancel_lbl = lv_label_create(cancel);
    lv_label_set_text(cancel_lbl, "CANCEL");
    lv_obj_set_style_text_color(cancel_lbl, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(cancel_lbl, font_14(), 0);
    lv_obj_center(cancel_lbl);

    lv_obj_t *ok = lv_button_create(row);
    lv_obj_set_size(ok, 120, 32);
    style_btn(ok, 0xB00020);
    lv_obj_align(ok, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(ok, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(kCmdFactoryReset)));
    lv_obj_t *ok_lbl = lv_label_create(ok);
    lv_label_set_text(ok_lbl, "RESET");
    lv_obj_set_style_text_color(ok_lbl, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(ok_lbl, font_14(), 0);
    lv_obj_center(ok_lbl);
}

void show_config_popup() {
    destroy_config();
    destroy_picker();

    s_config = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_config, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_config, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_config, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_config, 0, 0);
    lv_obj_set_style_pad_all(s_config, 0, 0);
    lv_obj_set_style_radius(s_config, 0, 0);
    lv_obj_clear_flag(s_config, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(
        s_config, [](lv_event_t *e) { (void)e; destroy_config(); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *panel = lv_obj_create(s_config);
    lv_obj_set_size(panel, 360, 280);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(kPanel), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(kMuted), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 10, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(panel, LV_DIR_VER);
    lv_obj_add_event_cb(
        panel, [](lv_event_t *e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED, nullptr);

    /* Header */
    lv_obj_t *hdr = lv_obj_create(panel);
    lv_obj_set_size(hdr, 336, 32);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_flex_grow(hdr, 0, 0);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(title, font_20(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *close_btn = lv_button_create(hdr);
    lv_obj_set_size(close_btn, 64, 28);
    style_btn(close_btn, kMuted);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(
        close_btn, [](lv_event_t *e) { (void)e; destroy_config(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "CLOSE");
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(close_lbl, font_14(), 0);
    lv_obj_center(close_lbl);

    /* 1) Brightness */
    lv_obj_t *br_hdr = lv_obj_create(panel);
    lv_obj_set_size(br_hdr, 336, 22);
    lv_obj_set_style_bg_opa(br_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(br_hdr, 0, 0);
    lv_obj_set_style_pad_all(br_hdr, 0, 0);
    lv_obj_clear_flag(br_hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *br_title = lv_label_create(br_hdr);
    lv_label_set_text(br_title, "Screen brightness");
    lv_obj_set_style_text_color(br_title, lv_color_hex(kTextMuted), 0);
    lv_obj_set_style_text_font(br_title, font_14(), 0);
    lv_obj_align(br_title, LV_ALIGN_LEFT_MID, 0, 0);

    int brightness = board::display::get_brightness();
    if (brightness < kBrightnessMin) {
        brightness = kBrightnessMin;
    }

    lv_obj_t *br_pct = lv_label_create(br_hdr);
    char pct_buf[16]{};
    std::snprintf(pct_buf, sizeof(pct_buf), "%d%%", brightness);
    lv_label_set_text(br_pct, pct_buf);
    lv_obj_set_style_text_color(br_pct, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(br_pct, font_14(), 0);
    lv_obj_align(br_pct, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *slider = lv_slider_create(panel);
    lv_obj_set_width(slider, 336);
    lv_obj_set_height(slider, 18);
    lv_slider_set_range(slider, kBrightnessMin, 100);
    lv_slider_set_value(slider, brightness, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kMuted), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kGreen), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(kTextLight), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 4, LV_PART_KNOB);
    lv_obj_add_event_cb(slider, on_brightness_changed, LV_EVENT_VALUE_CHANGED, br_pct);

    /* 2) System info */
    lv_obj_t *info_title = lv_label_create(panel);
    lv_label_set_text(info_title, "System info");
    lv_obj_set_style_text_color(info_title, lv_color_hex(kTextMuted), 0);
    lv_obj_set_style_text_font(info_title, font_14(), 0);
    lv_obj_set_width(info_title, 336);

    char info[256]{};
    fill_system_info(info, sizeof(info));
    lv_obj_t *info_lbl = lv_label_create(panel);
    lv_label_set_text(info_lbl, info);
    lv_obj_set_style_text_color(info_lbl, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(info_lbl, font_14(), 0);
    lv_obj_set_width(info_lbl, 336);

    /* 3) Reset */
    lv_obj_t *reset_btn = lv_button_create(panel);
    lv_obj_set_size(reset_btn, 336, 36);
    style_btn(reset_btn, 0xB00020);
    lv_obj_add_event_cb(
        reset_btn, [](lv_event_t *e) { (void)e; show_reset_confirm(); }, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *reset_lbl = lv_label_create(reset_btn);
    lv_label_set_text(reset_lbl, "RESET SYSTEM");
    lv_obj_set_style_text_color(reset_lbl, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(reset_lbl, font_14(), 0);
    lv_obj_center(reset_lbl);
}

void on_gear_clicked(lv_event_t *e) {
    (void)e;
    show_config_popup();
}

}  // namespace

void create_player() {
    destroy_picker();
    destroy_config();
    init_fonts();
    std::memset(&s_art_dsc, 0, sizeof(s_art_dsc));

    s_scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(kBg), 0);
    lv_obj_set_style_pad_all(s_scr, kPad, 0);
    lv_obj_set_style_pad_row(s_scr, kPad, 0);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *top = lv_obj_create(s_scr);
    lv_obj_set_size(top, 440, kArtH);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(top, kPad, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    s_art_frame = lv_obj_create(top);
    lv_obj_set_size(s_art_frame, kArtW, kArtH);
    lv_obj_set_style_radius(s_art_frame, 12, 0);
    lv_obj_set_style_clip_corner(s_art_frame, true, 0);
    lv_obj_set_style_bg_color(s_art_frame, lv_color_hex(kArtPlaceholder), 0);
    lv_obj_set_style_border_width(s_art_frame, 0, 0);
    lv_obj_set_style_pad_all(s_art_frame, 0, 0);
    lv_obj_clear_flag(s_art_frame, LV_OBJ_FLAG_SCROLLABLE);

    s_art_img = lv_image_create(s_art_frame);
    lv_obj_center(s_art_img);
    lv_obj_add_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);

    s_art_lbl = lv_label_create(s_art_frame);
    lv_label_set_text(s_art_lbl, "SPOTIFY");
    lv_obj_set_style_text_color(s_art_lbl, lv_color_hex(kTextDim), 0);
    lv_obj_set_style_text_font(s_art_lbl, font_14(), 0);
    lv_obj_center(s_art_lbl);

    lv_obj_t *right = lv_obj_create(top);
    lv_obj_set_size(right, 198, kArtH);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *name = lv_obj_create(right);
    lv_obj_set_width(name, 198);
    lv_obj_set_height(name, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(name, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(name, 0, 0);
    lv_obj_set_style_pad_all(name, 0, 0);
    lv_obj_set_flex_flow(name, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(name, 10, 0);
    lv_obj_clear_flag(name, LV_OBJ_FLAG_SCROLLABLE);

    s_title = lv_label_create(name);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_title, 198);
    lv_label_set_text(s_title, "Loading…");
    lv_obj_set_style_text_color(s_title, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(s_title, font_20(), 0);

    s_artist = lv_label_create(name);
    lv_label_set_long_mode(s_artist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_artist, 198);
    lv_label_set_text(s_artist, "");
    lv_obj_set_style_text_color(s_artist, lv_color_hex(kTextMuted), 0);
    lv_obj_set_style_text_font(s_artist, font_14(), 0);

    lv_obj_t *meta = lv_obj_create(right);
    lv_obj_set_width(meta, 198);
    lv_obj_set_height(meta, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(meta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(meta, 0, 0);
    lv_obj_set_style_pad_all(meta, 0, 0);
    lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(meta, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(meta, 11, 0);
    lv_obj_clear_flag(meta, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *gear_btn = lv_button_create(meta);
    lv_obj_set_size(gear_btn, ui_img_gear.header.w, ui_img_gear.header.h);
    lv_obj_set_style_bg_opa(gear_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gear_btn, 0, 0);
    lv_obj_set_style_shadow_width(gear_btn, 0, 0);
    lv_obj_set_style_pad_all(gear_btn, 0, 0);
    lv_obj_set_style_radius(gear_btn, 0, 0);
    lv_obj_set_style_margin_all(gear_btn, 0, 0);
    lv_obj_add_event_cb(gear_btn, on_gear_clicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *gear = lv_image_create(gear_btn);
    lv_image_set_src(gear, &ui_img_gear);
    lv_obj_set_size(gear, ui_img_gear.header.w, ui_img_gear.header.h);
    set_icon_color(gear, kIconIdle);
    lv_obj_align(gear, LV_ALIGN_LEFT_MID, 0, 0);

    s_device_btn = lv_button_create(meta);
    lv_obj_set_size(s_device_btn, 198, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_device_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_device_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_device_btn, 0, 0);
    lv_obj_set_style_pad_all(s_device_btn, 0, 0);
    lv_obj_set_style_radius(s_device_btn, 0, 0);
    lv_obj_set_style_margin_all(s_device_btn, 0, 0);
    lv_obj_add_event_cb(s_device_btn, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(kCmdOpenDevices)));

    s_device_lbl = lv_label_create(s_device_btn);
    lv_label_set_long_mode(s_device_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_device_lbl, 198);
    /* Use LV_SYMBOL_* / ASCII — Montserrat has no Unicode ↓ / em-dash glyphs. */
    lv_label_set_text(s_device_lbl, "Device: - " LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(s_device_lbl, lv_color_hex(kTextDim), 0);
    lv_obj_set_style_text_font(s_device_lbl, font_14(), 0);
    lv_obj_set_style_text_align(s_device_lbl, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(s_device_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *down = lv_obj_create(s_scr);
    lv_obj_set_size(down, 440, kControlsH);
    lv_obj_set_style_bg_opa(down, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(down, 0, 0);
    lv_obj_set_style_pad_all(down, 0, 0);
    lv_obj_set_flex_flow(down, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(down, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(down, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *controls = lv_obj_create(down);
    lv_obj_set_size(controls, kArtW, kControlsH);
    lv_obj_set_style_bg_opa(controls, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_pad_all(controls, 0, 0);
    lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *prev = make_icon_btn(controls, &ui_img_prev, 50, 50, &s_prev_img);
    lv_obj_add_event_cb(prev, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(3)));
    attach_press_flash(prev, s_prev_img);

    s_play_btn = make_icon_btn(controls, &ui_img_play, 50, 50, &s_play_img);
    lv_obj_add_event_cb(s_play_btn, on_play_clicked, LV_EVENT_CLICKED, nullptr);
    attach_press_flash(s_play_btn, s_play_img);

    lv_obj_t *next = make_icon_btn(controls, &ui_img_next, 50, 50, &s_next_img);
    lv_obj_add_event_cb(next, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(4)));
    attach_press_flash(next, s_next_img);

    lv_obj_t *other = lv_obj_create(down);
    lv_obj_set_size(other, 130, kControlsH);
    lv_obj_set_style_bg_opa(other, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(other, 0, 0);
    lv_obj_set_style_pad_all(other, 0, 0);
    lv_obj_set_flex_flow(other, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(other, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(other, LV_OBJ_FLAG_SCROLLABLE);

    s_like_btn = make_icon_btn(other, &ui_img_heart, 50, 50, &s_like_img);
    lv_obj_add_event_cb(s_like_btn, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(6)));

    s_shuf_btn = make_icon_btn(other, &ui_img_shuffle, 50, 50, &s_shuf_img);
    lv_obj_add_event_cb(s_shuf_btn, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(5)));

    sync_play_icon();
    sync_toggle_btns();
    lv_screen_load(s_scr);
}

void update_player(const PlayerView &view) {
    if (s_title) {
        lv_label_set_text(s_title, view.title[0] ? view.title : "Nothing playing");
    }
    if (s_artist) {
        lv_label_set_text(s_artist, view.artist);
    }
    if (s_device_lbl) {
        char line[64]{};
        std::snprintf(line, sizeof(line), "Device: %s " LV_SYMBOL_DOWN,
                      view.device[0] ? view.device : "-");
        lv_label_set_text(s_device_lbl, line);
    }
    s_playing = view.is_playing;
    s_shuffle = view.shuffle;
    s_liked = view.liked;
    sync_play_icon();
    sync_toggle_btns();
}

void set_player_playing(bool playing) {
    s_playing = playing;
    sync_play_icon();
}

void set_player_shuffle(bool on) {
    s_shuffle = on;
    sync_toggle_btns();
}

void set_player_liked(bool on) {
    s_liked = on;
    sync_toggle_btns();
}

void set_player_cover(const uint16_t *rgb565, uint16_t w, uint16_t h) {
    if (!s_art_img || !rgb565 || w == 0 || h == 0) {
        clear_player_cover();
        return;
    }

    s_art_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_art_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_art_dsc.header.w = w;
    s_art_dsc.header.h = h;
    s_art_dsc.header.stride = static_cast<uint32_t>(w) * 2;
    s_art_dsc.data_size = static_cast<uint32_t>(w) * h * 2;
    s_art_dsc.data = reinterpret_cast<const uint8_t *>(rgb565);

    lv_image_set_src(s_art_img, &s_art_dsc);
    lv_obj_set_size(s_art_img, w, h);
    /* Fit into the rounded art frame (cover). */
    const int scale_w = (kArtW * 256) / static_cast<int>(w);
    const int scale_h = (kArtH * 256) / static_cast<int>(h);
    const int scale = scale_w > scale_h ? scale_w : scale_h;
    lv_image_set_scale(s_art_img, static_cast<uint32_t>(scale > 0 ? scale : 256));
    lv_obj_center(s_art_img);
    lv_obj_clear_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);
    if (s_art_lbl) {
        lv_obj_add_flag(s_art_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void clear_player_cover() {
    std::memset(&s_art_dsc, 0, sizeof(s_art_dsc));
    if (s_art_img) {
        lv_image_set_src(s_art_img, nullptr);
        lv_obj_add_flag(s_art_img, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_art_lbl) {
        lv_obj_clear_flag(s_art_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

void show_device_picker(const spotify::DeviceList &devices) {
    destroy_config();
    destroy_picker();
    s_picker_devices = devices;

    s_picker = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_picker, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_picker, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_picker, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_picker, 0, 0);
    lv_obj_set_style_pad_all(s_picker, 0, 0);
    lv_obj_set_style_radius(s_picker, 0, 0);
    lv_obj_clear_flag(s_picker, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_picker, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(kCmdCloseDevices)));

    lv_obj_t *panel = lv_obj_create(s_picker);
    lv_obj_set_size(panel, 360, 260);
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(kPanel), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(kMuted), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, 8, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(
        panel, [](lv_event_t *e) { lv_event_stop_bubbling(e); }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *hdr = lv_obj_create(panel);
    lv_obj_set_size(hdr, 336, 32);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "Devices");
    lv_obj_set_style_text_color(title, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(title, font_20(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *close_btn = lv_button_create(hdr);
    lv_obj_set_size(close_btn, 64, 28);
    style_btn(close_btn, kMuted);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, on_cmd_clicked, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(kCmdCloseDevices)));
    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "CLOSE");
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(kTextLight), 0);
    lv_obj_set_style_text_font(close_lbl, font_14(), 0);
    lv_obj_center(close_lbl);

    lv_obj_t *list = lv_obj_create(panel);
    lv_obj_set_size(list, 336, 190);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 6, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    if (devices.count == 0) {
        lv_obj_t *empty = lv_label_create(list);
        lv_label_set_text(empty, "No devices online.\nOpen Spotify on a speaker or phone.");
        lv_obj_set_style_text_color(empty, lv_color_hex(kTextMuted), 0);
        lv_obj_set_style_text_font(empty, font_14(), 0);
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(empty, 320);
        return;
    }

    for (int i = 0; i < devices.count; ++i) {
        const spotify::Device &d = devices.items[i];
        lv_obj_t *row = lv_button_create(list);
        lv_obj_set_size(row, 336, 40);
        style_btn(row, d.is_active ? kGreen : kMuted);
        lv_obj_add_event_cb(row, on_device_row_clicked, LV_EVENT_CLICKED,
                            reinterpret_cast<void *>(static_cast<intptr_t>(i)));

        char label[80]{};
        std::snprintf(label, sizeof(label), "%s%s", d.name[0] ? d.name : "Unknown",
                      d.is_active ? "  *" : "");
        lv_obj_t *lbl = lv_label_create(row);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, 320);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(d.is_active ? kTextDark : kTextLight), 0);
        lv_obj_set_style_text_font(lbl, font_14(), 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
    }
}

void hide_device_picker() {
    destroy_picker();
}

void hide_config_popup() {
    destroy_config();
}

bool take_selected_device_id(char *out, size_t out_len) {
    if (!out || out_len == 0 || s_selected_device_id[0] == '\0') {
        return false;
    }
    std::snprintf(out, out_len, "%s", s_selected_device_id);
    s_selected_device_id[0] = '\0';
    return true;
}

PlayerCommand take_player_command() {
    const int cmd = s_cmd;
    s_cmd = 0;
    switch (cmd) {
    case 1:
        return PlayerCommand::Play;
    case 2:
        return PlayerCommand::Pause;
    case 3:
        return PlayerCommand::Prev;
    case 4:
        return PlayerCommand::Next;
    case 5:
        return PlayerCommand::ShuffleToggle;
    case 6:
        return PlayerCommand::LikeToggle;
    case kCmdOpenDevices:
        return PlayerCommand::OpenDevices;
    case kCmdCloseDevices:
        return PlayerCommand::CloseDevices;
    case kCmdFactoryReset:
        return PlayerCommand::FactoryReset;
    case kCmdSelectBase:
        return PlayerCommand::SelectDevice;
    default:
        return PlayerCommand::None;
    }
}

}  // namespace ui
