#include "streamdeck.h"
#include "lvgl.h"
#include <esp/display.h>
#include <string.h>

// // Definições de tamanho e quantidade de botões
// #define BUTTON_WIDTH 80
// #define BUTTON_HEIGHT 80
// #define BUTTON_COUNT 5

// // Estado da aplicação
// int brightness = 30;

// static lv_style_t container_style;

// // Variáveis globais para a label e o timer
// static lv_obj_t *brightness_label = NULL;
// static lv_timer_t *hide_timer = NULL;

// // Função para ocultar a label de brilho
// static void hide_brightness_label_cb(lv_timer_t *timer) {
//     lv_obj_add_flag(brightness_label, LV_OBJ_FLAG_HIDDEN);
//     lv_timer_del(hide_timer);
//     hide_timer = NULL;
// }

// // Função para mostrar a label de brilho
// void show_brightness(int value) {
//     char buf[13];
//     lv_snprintf(buf, sizeof(buf), "Brilho: %d%%", value);

//     lv_label_set_text(brightness_label, buf);
//     lv_obj_clear_flag(brightness_label, LV_OBJ_FLAG_HIDDEN);

//     if (hide_timer) {
//         lv_timer_reset(hide_timer);
//     } else {
//         hide_timer = lv_timer_create(hide_brightness_label_cb, 1500, NULL);
//     }
// }

// // Função de fábrica para preencher uma estrutura bnt_action
// void create_button_data(bnt_action *button, const char *label, ACTION_TYPE type, const char *action) {
//     strcpy(button->label, label);
//     button->ACTION_TYPE = type;
//     strcpy(button->action, action);
// }

// // Funções para simular a leitura do SD Card e obter as ações dos botões
// void get_button_actions(bnt_action buttons[]) {
//     // Simulando a leitura do arquivo CSV do SD Card
//     create_button_data(&buttons[0], "B+", SYSTEM, "BRIGHTNESS_UP");
//     create_button_data(&buttons[1], "B-", SYSTEM, "BRIGHTNESS_DOWN");
//     create_button_data(&buttons[2], "Teste 1", SHORTCUT, "TEST_1");
//     create_button_data(&buttons[3], "Teste 2", SHORTCUT, "TEST_2");
//     create_button_data(&buttons[4], "Teste 3", SHORTCUT, "TEST_3");
// }

// static void button_event_handler(lv_event_t *event) {
//     lv_obj_t *btn = lv_event_get_target(event);
//     bnt_action *action_data = (bnt_action *)lv_obj_get_user_data(btn);

//     if (!action_data)
//         return;

//     switch (action_data->ACTION_TYPE) {
//     case SHORTCUT:
//         printf("Acao de atalho: %s\n", action_data->action);
//         break;
//     case SYSTEM:
//         if (strcmp(action_data->action, "BRIGHTNESS_UP") == 0) {
//             if (brightness >= 100)
//                 return;
//             bsp_display_brightness_set((brightness += 5));
//             // show_brightness(brightness);
//         } else if (strcmp(action_data->action, "BRIGHTNESS_DOWN") == 0) {
//             if (brightness <= 0)
//                 return;
//             bsp_display_brightness_set((brightness -= 5));
//             // show_brightness(brightness);
//         }
//         break;
//     default:
//         printf("Tipo de acao nao reconhecida.\n");
//         break;
//     }
// }

// lv_obj_t *create_button(lv_obj_t *parent, bnt_action *action_data) {
//     lv_obj_t *btn = lv_btn_create(parent);
//     lv_obj_set_size(btn, BUTTON_WIDTH, BUTTON_HEIGHT);

//     lv_obj_set_style_bg_color(btn, lv_color_hex(0x202121), LV_PART_MAIN);
//     lv_obj_set_style_shadow_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);
//     lv_obj_set_style_shadow_width(btn, 10, LV_PART_MAIN);
//     lv_obj_set_style_border_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN);
//     lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
//     lv_obj_set_style_bg_color(btn, lv_color_hex(0x282a2c), LV_PART_CURSOR);

//     lv_obj_t *label = lv_label_create(btn);
//     lv_label_set_text(label, action_data->label);
//     lv_obj_center(label);

//     lv_obj_set_user_data(btn, (void *)action_data);

//     lv_obj_add_event_cb(btn, button_event_handler, LV_EVENT_CLICKED, NULL);

//     return btn;
// }

// void init_styles() {
//     lv_style_init(&container_style);

//     // Configura as propriedades de estilo
//     lv_style_set_flex_flow(&container_style, LV_FLEX_FLOW_ROW_WRAP);
//     lv_style_set_bg_color(&container_style, lv_color_hex(0x000000));
//     lv_style_set_border_width(&container_style, 0);
//     lv_style_set_pad_all(&container_style, 0);
//     lv_style_set_pad_row(&container_style, 10);
//     lv_style_set_pad_column(&container_style, 10);
// }

// void create_streamdeck_ui() {
//     bsp_display_brightness_set(brightness);

//     bnt_action buttons[BUTTON_COUNT];
//     get_button_actions(buttons);

//     lv_obj_t *screen = lv_scr_act();
//     lv_obj_set_style_bg_color(screen, lv_color_hex(0x202121), LV_PART_MAIN);

//     // Contêiner principal para empilhar elementos
//     lv_obj_t *main_container = lv_obj_create(screen);
//     // Aplica o estilo, mas substitui as propriedades necessárias
//     lv_obj_add_style(main_container, &container_style, LV_PART_MAIN);
//     lv_obj_set_size(main_container, LV_PCT(100), LV_PCT(100)); // Tamanho completo
//     lv_obj_set_flex_align(main_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

//     // Contêiner para a label de brilho
//     lv_obj_t *brightness_container = lv_obj_create(main_container);
//     // Aplica o estilo padrão
//     lv_obj_add_style(brightness_container, &container_style, LV_PART_MAIN);
//     lv_obj_set_flex_align(brightness_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

//     // Contêiner para os botões
//     lv_obj_t *buttons_container = lv_obj_create(main_container);
//     // Aplica o estilo padrão
//     lv_obj_add_style(buttons_container, &container_style, LV_PART_MAIN);
//     lv_obj_set_flex_align(buttons_container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

//     // // Cria a label de brilho e a esconde
//     // brightness_label = lv_label_create(brightness_container);
//     // lv_obj_set_style_text_color(brightness_label, lv_color_white(), 0);
//     // lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_36, 0);
//     // lv_label_set_text(brightness_label, "");
//     // lv_obj_add_flag(brightness_label, LV_OBJ_FLAG_HIDDEN);

//     // Loop para criar os botões
//     for (int i = 0; i < BUTTON_COUNT; i++) {
//         create_button(buttons_container, &buttons[i]);
//     }
// }

static lv_style_t style_container;

void init_styles() {
    lv_style_init(&style_container);
    lv_style_set_bg_opa(&style_container, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_container, 0);
    lv_style_set_pad_all(&style_container, 0);
    lv_style_set_pad_row(&style_container, 0);
    lv_style_set_pad_column(&style_container, 0);
}

void create_streamdeck_ui() {
    bsp_display_brightness_set(30);

    // --- Tela Raiz ---
    lv_obj_t *screen = lv_scr_act();
    lv_obj_add_style(screen, &style_container, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x00FF00), LV_PART_MAIN);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    
    // --- Container Superior (TopRow) ---
    lv_obj_t *top_row_container = lv_obj_create(screen);
    lv_obj_set_size(top_row_container, LV_PCT(100), 54);
    lv_obj_set_style_radius(top_row_container, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(top_row_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_row_container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // lv_obj_set_style_pad_all(top_row_container, 10, LV_PART_MAIN);

    // --- Container dos Botões (ButtonRow) ---
    lv_obj_t *button_row_container = lv_obj_create(top_row_container);
    lv_obj_set_size(button_row_container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(button_row_container, LV_FLEX_FLOW_ROW);
    // lv_obj_set_style_pad_all(top_row_container, 10, 0);
    lv_obj_set_style_bg_opa(button_row_container, LV_OPA_TRANSP, 0); // Torna o container transparente
    lv_obj_set_style_border_width(button_row_container, 0, 0);       // Remove a borda
    lv_obj_set_style_pad_all(button_row_container, 0, 0);            // Remove o padding para aninhar os botões

    // Botão 1
    lv_obj_t *btn1 = lv_btn_create(button_row_container);
    lv_obj_set_size(btn1, 35, 35);
    lv_obj_set_style_radius(btn1, 4, 0);

    // Botão 2
    lv_obj_t *btn2 = lv_btn_create(button_row_container);
    lv_obj_set_size(btn2, 35, 35);
    lv_obj_set_style_radius(btn2, 4, 0);

    // --- Label no Container Superior ---
    lv_obj_t *label_top = lv_label_create(top_row_container);
    lv_label_set_text(label_top, "Brilho: 30%");
    lv_obj_set_style_text_font(label_top, &lv_font_montserrat_16, 0);

    // --- Container de Conteúdo Principal (MainContent) ---
    lv_obj_t *main_content_container = lv_obj_create(screen);
    lv_obj_set_size(main_content_container, LV_PCT(100), LV_PCT(100)); // Preenche o espaço restante
    lv_obj_set_flex_flow(main_content_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_grow(main_content_container, 1); // Permite que ele cresça e preencha a altura
    lv_obj_set_style_pad_all(main_content_container, 10, 0);

    // --- Botões do Conteúdo Principal ---
    for (int i = 0; i < 5; i++) {
        lv_obj_t *main_btn = lv_btn_create(main_content_container);
        lv_obj_set_size(main_btn, 80, 80);
        lv_obj_set_style_radius(main_btn, 4, 0);
        lv_obj_t *label = lv_label_create(main_btn);
        lv_label_set_text_fmt(label, "Btn %d", i + 1);
        lv_obj_center(label);
    }
}