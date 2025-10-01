#include "buttons.h"

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