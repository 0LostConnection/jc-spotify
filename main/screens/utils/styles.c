#include "styles.h"


// Estilos globais
lv_style_t style_reset_container;
lv_style_t style_buttons;

void init_styles() {
    // --- Style Reset Container ---
    lv_style_init(&style_reset_container);
    lv_style_set_bg_opa(&style_reset_container, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_reset_container, 0);
    lv_style_set_pad_all(&style_reset_container, 0);
    lv_style_set_pad_row(&style_reset_container, 0);
    lv_style_set_pad_column(&style_reset_container, 0);
    lv_style_set_radius(&style_reset_container, 0);

    // --- Style Buttons ---
    lv_style_init(&style_buttons);
    lv_style_set_bg_color(&style_buttons, lv_color_hex(0x282a2c));
    lv_style_set_shadow_color(&style_buttons, lv_color_hex(0xffffff));
    lv_style_set_shadow_width(&style_buttons, 10);
    lv_style_set_border_color(&style_buttons, lv_color_hex(0xffffff));
    lv_style_set_border_width(&style_buttons, 1);
    lv_style_set_radius(&style_buttons, 4);
}