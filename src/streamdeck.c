#include <display.h>
#include <streamdeck.h>

#define BUTTON_WIDTH 80
#define BUTTON_HEIGHT 80
#define BUTTON_COUNT 5

// Função de fábrica para preencher uma estrutura bnt_action
void create_button_data(bnt_action *button, const char *label, int position, ACTION_TYPE type, const char *action) {
    strcpy(button->label, label);
    button->position = position;
    button->ACTION_TYPE = type;
    strcpy(button->action, action);
}

void get_button_actions(bnt_action buttons[]) {
    // Simulando a leitra do arquivo CSV do SD Card
    // Este array de estruturas representa o conteúdo que seria lido do arquivo.

    create_button_data(&buttons[0], "Volume +", 0, SHORTCUT, "VOLUME_UP");
    create_button_data(&buttons[1], "Volume -", 1, SHORTCUT, "VOLUME_DOWN");
    create_button_data(&buttons[2], "Teste 1", 2, SHORTCUT, "TEST");
    create_button_data(&buttons[3], "Teste 2", 3, SHORTCUT, "TEST");
    create_button_data(&buttons[4], "Teste 3", 4, SHORTCUT, "TEST");
}

lv_obj_t *create_button(lv_obj_t *parent, bnt_action *action_data) {
    // Cria o objeto do botão
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, BUTTON_WIDTH, BUTTON_HEIGHT);

    // Estilo dos botões
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x202121), LV_PART_MAIN);     // Fundo
    lv_obj_set_style_shadow_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN); // Sombra
    lv_obj_set_style_shadow_width(btn, 10, LV_PART_MAIN);                     // Sombra
    lv_obj_set_style_border_color(btn, lv_color_hex(0xffffff), LV_PART_MAIN); // Borda
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);                      // Borda

    // Estilo quando pressionado
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x282a2c), LV_PART_CURSOR);

    // Cria o rótulo dentro do botão e define o texto
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, action_data->label);
    lv_obj_center(label);

    // Salva os dados da ação no botão
    lv_obj_set_user_data(btn, (void *)action_data);

    return btn;
}

void create_streamdeck_ui() {
    // Configura o display
    bsp_display_brightness_set(30);

    bnt_action buttons[BUTTON_COUNT];
    get_button_actions(buttons);

    // Tela ativa
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x202121), LV_PART_MAIN);

    // Criar obj container
    lv_obj_t *container = lv_obj_create(screen);

    // Configura container
    lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));   // Ocupa 100% da tela
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW_WRAP); // Alinha em linhas e quebra para a próxima

    // Centraliza horizontal e verticalmente os botões
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Paddings
    lv_obj_set_style_pad_all(container, 10, LV_PART_MAIN);    // Espaço inteiro
    lv_obj_set_style_pad_row(container, 10, LV_PART_MAIN);    // Espaço entre as linhas
    lv_obj_set_style_pad_column(container, 10, LV_PART_MAIN); // Espaço entre as colunas

    // Remove o estilo padrão do contêiner (borda, fundo, etc.)
    lv_obj_set_style_bg_color(container, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_border_width(container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(container, 0, LV_PART_MAIN);

    // Loop para criar os botões
    for (int i = 0; i < BUTTON_COUNT; i++) {
        create_button(container, &buttons[i]);
    }
}