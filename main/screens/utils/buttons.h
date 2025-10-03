#ifndef BUTTONS_H
#define BUTTONS_H

#include "lvgl.h"

typedef enum {
    SCREEN = 0,
    SYSTEM = 1,
    SHORTCUT = 2
} ACTION_TYPE;

typedef struct {
    char label[50];
    ACTION_TYPE type;
    char action[50];
} bnt_action;

void get_button_actions(bnt_action buttons[]);

void create_button_data(bnt_action *button, const char *label, ACTION_TYPE type, const char *action);

lv_obj_t *create_button(lv_obj_t *parent, lv_event_cb_t callback_function, bnt_action *action_data, lv_coord_t width, lv_coord_t height);

// void get_button_actions(bnt_action buttons[]);

#endif // STREAMDECK_H