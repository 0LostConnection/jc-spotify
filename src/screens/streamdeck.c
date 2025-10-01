#include "streamdeck.h"
#include "lvgl.h"
#include <esp/display.h>
#include <string.h>

// Definições de tamanho e quantidade de botões
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 80
#define BUTTON_COUNT 5

// Estado da aplicação
int brightness = 30;

// --- STYLES ---
static lv_style_t style_reset_container;
static lv_style_t style_buttons;

// --- LABELS ---
static lv_obj_t *brightness_label = NULL;

// --- INICIO MOCK ---

// Função de fábrica para preencher uma estrutura bnt_action
void create_button_data(bnt_action *button, const char *label, ACTION_TYPE type, const char *action) {
    strcpy(button->label, label);
    button->ACTION_TYPE = type;
    strcpy(button->action, action);
}

// Funções para simular a leitura do SD Card e obter as ações dos botões
void get_button_actions(bnt_action buttons[]) {
    // Simulando a leitura do arquivo CSV do SD Card
    create_button_data(&buttons[0], "B+", SYSTEM, "BRIGHTNESS_UP");
    create_button_data(&buttons[1], "B-", SYSTEM, "BRIGHTNESS_DOWN");
    create_button_data(&buttons[2], "Teste 1", SHORTCUT, "TEST_1");
    create_button_data(&buttons[3], "Teste 2", SHORTCUT, "TEST_2");
    create_button_data(&buttons[4], "Teste 3", SHORTCUT, "TEST_3");
}

// --- FIM MOCK ---

void update_brightness_label(int value) {
    char buf[13];
    lv_snprintf(buf, sizeof(buf), "Brilho: %d%%", value);
    lv_label_set_text(brightness_label, buf);
}

static void button_event_handler(lv_event_t *event) {
    lv_obj_t *btn = lv_event_get_target(event);
    bnt_action *action_data = (bnt_action *)lv_obj_get_user_data(btn);

    if (!action_data)
        return;

    switch (action_data->ACTION_TYPE) {
    case SHORTCUT:
        printf("Acao de atalho: %s\n", action_data->action);
        break;
    case SYSTEM:
        if (strcmp(action_data->action, "BRIGHTNESS_UP") == 0) {
            if (brightness >= 100)
                return;
            bsp_display_brightness_set((brightness += 5));
            update_brightness_label(brightness);
        } else if (strcmp(action_data->action, "BRIGHTNESS_DOWN") == 0) {
            if (brightness <= 0)
                return;
            bsp_display_brightness_set((brightness -= 5));
            update_brightness_label(brightness);
            
        }
        break;
    default:
        printf("Tipo de acao nao reconhecida.\n");
        break;
    }
}

lv_obj_t *create_button(lv_obj_t *parent, bnt_action *action_data, lv_coord_t width, lv_coord_t height) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_add_style(btn, &style_buttons, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, action_data->label);
    lv_obj_center(label);

    lv_obj_set_user_data(btn, (void *)action_data);

    lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_CLICKED, NULL);

    return btn;
}

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

void create_streamdeck_ui() {
    // --- Init Styles ---
    init_styles();

    // --- Get botões
    bnt_action buttons[BUTTON_COUNT];
    get_button_actions(buttons);

    // --- INICIO DA CRIAÇÃO DA TELA ---

    // --- Tela Raiz ---
    lv_obj_t *screen = lv_scr_act();
    lv_obj_add_style(screen, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x202121), LV_PART_MAIN);
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

    // --- Label Brilho ---
    brightness_label = lv_label_create(top_row_container);
    lv_obj_set_style_text_color(brightness_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(brightness_label, "Brilho: 30%");
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_18, 0);

    // --- Container de Conteúdo Principal (MainContent) ---
    lv_obj_t *main_content_container = lv_obj_create(screen);
    lv_obj_add_style(main_content_container, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_size(main_content_container, LV_PCT(100), LV_PCT(100)); // Preenche o espaço restante
    lv_obj_set_flex_align(main_content_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_flow(main_content_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_grow(main_content_container, 1); // Permite que ele cresça e preencha a altura
    lv_obj_set_style_pad_row(main_content_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(main_content_container, 10, LV_PART_MAIN);

    // --- Loop para a Criação dos Botões ---

    for (int i = 0; i < BUTTON_COUNT; i++) {
        switch (buttons[i].ACTION_TYPE) {
        case SYSTEM:
            create_button(button_row_container, &buttons[i], 34, 34);
            break;
        case SHORTCUT:
            create_button(main_content_container, &buttons[i], 80, 80);
            break;
        default:
            continue;
        }
    }

    // --- FIM DA CRIAÇÃO DA TELA ---

    // --- Configure Display Brightness and Update Label ---
    bsp_display_brightness_set(brightness);
    update_brightness_label(brightness);
}