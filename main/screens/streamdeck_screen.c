#include "streamdeck_screen.h"

// Definições de tamanho e quantidade de botões
#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 80
#define BUTTON_COUNT 9

// --- Buttons ---
static bnt_action buttons[BUTTON_COUNT];

static lv_obj_t *test_label;

// --- INICIO MOCK ---

static void button_event_handler(lv_event_t *event) {
    lv_obj_t *btn = lv_event_get_target(event);
    bnt_action *action_data = (bnt_action *)lv_obj_get_user_data(btn);
    if (!action_data)
        return;

    switch (action_data->type) {
    case SCREEN: {
        lv_obj_t *config_screen = create_config_ui();
        lv_disp_load_scr(config_screen);
    } break;
    case SHORTCUT: {
        uint8_t keys[] = {HID_KEY_VOLUME_UP}; // DEL
        lv_label_set_text(test_label, hid_send_shortcut(0, keys, 1) ? "true" : "false");
    } break;
    case SYSTEM: {
        if (strcmp(action_data->action, "BRIGHTNESS_UP") == 0) {
            // ...
        } else if (strcmp(action_data->action, "BRIGHTNESS_DOWN") == 0) {
            // ...
        }
    } break;
    default:
        printf("'%d' - '%s': Tipo de ação nao reconhecida.\n",
               action_data->type, action_data->action);
        break;
    }
}

lv_obj_t *create_streamdeck_ui() {
    // --- Init Styles ---
    init_styles();

    // --- Get botões
    get_button_actions(buttons);

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

    // --- Container de Conteúdo Principal (MainContent) ---
    lv_obj_t *main_content_container = lv_obj_create(screen);
    lv_obj_add_style(main_content_container, &style_reset_container, LV_PART_MAIN);
    lv_obj_set_size(main_content_container, LV_PCT(100), LV_PCT(100)); // Preenche o espaço restante
    lv_obj_set_flex_align(main_content_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_flow(main_content_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_grow(main_content_container, 1); // Permite que ele cresça e preencha a altura
    lv_obj_set_style_pad_row(main_content_container, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_column(main_content_container, 10, LV_PART_MAIN);

    test_label = lv_label_create(button_row_container);

    // --- Loop para a Criação dos Botões ---

    for (int i = 0; i < BUTTON_COUNT; i++) {
        switch (buttons[i].type) {
        case SCREEN:
            create_button(button_row_container, button_event_handler, &buttons[i], 34, 34);
            break;
        case SHORTCUT:
            create_button(main_content_container, button_event_handler, &buttons[i], 80, 80);
            break;
        default:
            continue;
        }
    }
    // --- FIM DA CRIAÇÃO DA TELA ---

    return screen;
}