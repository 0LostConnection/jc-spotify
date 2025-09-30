#ifndef STREAMDECK_H
#define STREAMDECK_H

#include "lvgl.h"
#include <display.h>

typedef enum {
    SHORTCUT = 0
} ACTION_TYPE;

typedef struct {
    char label[50];
    int position;
    ACTION_TYPE ACTION_TYPE;
    char action[50];
} bnt_action;

void create_button_data(bnt_action *button, const char *label, int position, ACTION_TYPE type, const char *action);
void get_button_actions(bnt_action buttons[]);


void create_streamdeck_ui(void);

#endif // STREAMDECK_H