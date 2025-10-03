#include "config_screen.h"
#include "esp/display.h"
#include "streamdeck_screen.h"
#include "utils/buttons.h"
#include "utils/styles.h"
#include <string.h>

// Definições de tamanho e quantidade de botões
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 80
#define BUTTON_COUNT 10

// Estado da aplicação
int brightness = 30;

// --- LABELS ---
static lv_obj_t *brightness_label = NULL;

// --- FIM MOCK ---

void update_brightness_label(int value) {
    char buf[20];
    lv_snprintf(buf, sizeof(buf), "Brilho: %d%%", value);
    lv_label_set_text(brightness_label, buf);
}

static void brightness_button_event_cb(lv_event_t *event) {
    lv_obj_t *btn = lv_event_get_target(event);
    const char *label = lv_label_get_text(lv_obj_get_child(btn, 0));
    if (strcmp(label, "B+") == 0) {
        if (brightness < 100) {
            brightness += 5;
            if (brightness > 100)
                brightness = 100;
        }
    } else if (strcmp(label, "B-") == 0) {
        if (brightness > 0) {
            brightness -= 5;
            if (brightness < 0)
                brightness = 0;
        }
    }
    bsp_display_brightness_set(brightness);
    update_brightness_label(brightness);
}

static void nav_button_event_cb(lv_event_t *event) {
    printf("Navegar para outra página\n");

    lv_obj_t *streamdeck_screen = create_streamdeck_ui();
    lv_disp_load_scr(streamdeck_screen);
}

lv_obj_t *create_config_ui() {
    // --- Init Styles ---
    init_styles();

    // --- INICIO DA CRIAÇÃO DA TELA ---

    // --- Tela Raiz ---
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_add_style(screen, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);

    // --- Container Superior (TopRow) ---
    lv_obj_t *top_row_container = lv_obj_create(screen);
    lv_obj_add_style(top_row_container, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_size(top_row_container, LV_PCT(100), 54);
    lv_obj_set_flex_flow(top_row_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(top_row_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_left(top_row_container, 20, LV_PART_MAIN);
    lv_obj_set_style_pad_right(top_row_container, 20, LV_PART_MAIN);

    // --- Container dos Botões (ButtonRow) ---
    lv_obj_t *button_row_container = lv_obj_create(top_row_container);
    lv_obj_add_style(button_row_container, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_size(button_row_container, LV_PCT(100), 34);
    lv_obj_set_flex_flow(button_row_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(button_row_container, 10, LV_PART_MAIN);

    // --- Botão de Navegação ---
    lv_obj_t *nav_btn = lv_btn_create(button_row_container);
    lv_obj_add_style(nav_btn, &style_buttons, LV_PART_MAIN);
    lv_obj_set_size(nav_btn, 34, 34);
    lv_obj_t *nav_label = lv_label_create(nav_btn);
    lv_label_set_text(nav_label, "S");
    lv_obj_center(nav_label);
    lv_obj_add_event_cb(nav_btn, nav_button_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Container de Conteúdo Principal (MainContent) ---
    lv_obj_t *main_content_container = lv_obj_create(screen);
    lv_obj_add_style(main_content_container, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_size(main_content_container, LV_PCT(100), LV_PCT(100)); // Preenche o espaço restante
    lv_obj_set_flex_align(main_content_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_flow(main_content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(main_content_container, 1); // Permite que ele cresça e preencha a altura
    lv_obj_set_style_pad_row(main_content_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(main_content_container, 10, LV_PART_MAIN);

    // --- Label Brilho ---
    brightness_label = lv_label_create(main_content_container);
    lv_obj_set_style_text_color(brightness_label, lv_color_white(), LV_PART_MAIN);
    // lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_18, 0);
    update_brightness_label(brightness);

    // --- Botões de Brilho ---
    lv_obj_t *row = lv_obj_create(main_content_container);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(row, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);

    lv_obj_t *btn_plus = lv_btn_create(row);
    lv_obj_set_size(btn_plus, 60, 60);
    lv_obj_add_style(btn_plus, &style_buttons, LV_PART_MAIN);
    lv_obj_t *lbl_plus = lv_label_create(btn_plus);
    lv_label_set_text(lbl_plus, "B+");
    lv_obj_center(lbl_plus);
    lv_obj_add_event_cb(btn_plus, brightness_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_minus = lv_btn_create(row);
    lv_obj_set_size(btn_minus, 60, 60);
    lv_obj_add_style(btn_minus, &style_buttons, LV_PART_MAIN);
    lv_obj_t *lbl_minus = lv_label_create(btn_minus);
    lv_label_set_text(lbl_minus, "B-");
    lv_obj_center(lbl_minus);
    lv_obj_add_event_cb(btn_minus, brightness_button_event_cb, LV_EVENT_CLICKED, NULL);

    // --- FIM DA CRIAÇÃO DA TELA ---

    // --- Configure Display Brightness and Update Label ---
    update_brightness_label(brightness);

    return screen;
}