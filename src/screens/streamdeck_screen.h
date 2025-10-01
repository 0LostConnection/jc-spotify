#ifndef STREAMDECK_SCREEN_H
#define STREAMDECK_SCREEN_H

#include "lvgl.h"
#include <esp/display.h>
#include <string.h>
#include "utils/buttons.h"
#include "utils/styles.h"
#include "config_screen.h"

lv_obj_t *create_streamdeck_ui(void);

#endif // STREAMDECK_H