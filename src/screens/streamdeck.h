#ifndef STREAMDECK_H
#define STREAMDECK_H

#include "lvgl.h"
#include <esp/display.h>

typedef enum {
    SYSTEM = 0,
    SHORTCUT = 1
} ACTION_TYPE;

typedef struct {
    char label[50];
    ACTION_TYPE ACTION_TYPE;
    char action[50];
} bnt_action;

void create_button_data(bnt_action *button, const char *label, ACTION_TYPE type, const char *action);
void get_button_actions(bnt_action buttons[]);


void create_streamdeck_ui(void);

#endif // STREAMDECK_H