#ifndef CONFIG_SCREEN_H
#define CONFIG_SCREEN_H

#include "lvgl.h"
#include "utils/buttons.h"
#include "utils/styles.h"
#include <esp/display.h>
#include <string.h>
#include "streamdeck_screen.h"

lv_obj_t *create_config_ui(void);

#endif // STREAMDECK_H