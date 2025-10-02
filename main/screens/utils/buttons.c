#include "buttons.h"

// Funções para simular a leitura do SD Card e obter as ações dos botões
void get_button_actions(bnt_action buttons[]) {
    // Simulando a leitura do arquivo CSV do SD Card
    create_button_data(&buttons[0], "C", SCREEN, "CONFIG");
    create_button_data(&buttons[1], "Teste 1", SHORTCUT, "A");
    create_button_data(&buttons[2], "Teste 2", SHORTCUT, "A");
    create_button_data(&buttons[3], "Teste 4", SHORTCUT, "A");
    create_button_data(&buttons[4], "Teste 5", SHORTCUT, "A");
    create_button_data(&buttons[5], "Teste 6", SHORTCUT, "A");
    create_button_data(&buttons[6], "Teste 7", SHORTCUT, "A");
    create_button_data(&buttons[7], "Teste 8", SHORTCUT, "A");
    create_button_data(&buttons[8], "Teste 9", SHORTCUT, "A");
}

void create_button_data(bnt_action *button, const char *label, ACTION_TYPE type, const char *action) {
    strcpy(button->label, label);
    button->type = type;
    strcpy(button->action, action);
}

// void get_button_actions(bnt_action buttons[]);

lv_obj_t *create_button(lv_obj_t *parent, lv_event_cb_t callback_function, bnt_action *action_data, lv_coord_t width, lv_coord_t height) {
    init_styles();

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_add_style(btn, &style_buttons, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, action_data->label);
    lv_obj_center(label);

    lv_obj_set_user_data(btn, (void *)action_data);

    lv_obj_add_event_cb(btn, callback_function, LV_EVENT_CLICKED, NULL);

    return btn;
}