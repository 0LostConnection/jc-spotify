#ifndef STREAMDECK_SCREEN_H
#define STREAMDECK_SCREEN_H

#include "config_screen.h"
#include "lvgl.h"
#include "utils/buttons.h"
#include "utils/styles.h"
#include <esp/display.h>
#include <string.h>
#include "utils/hid_util.h"
#include "class/hid/hid.h"

lv_obj_t *create_streamdeck_ui(void);

#endif // STREAMDECK_H