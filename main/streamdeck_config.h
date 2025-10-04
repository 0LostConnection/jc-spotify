#ifndef STREAMDECK_CONF_H
#define STREAMDECK_CONF_H

#include <stdbool.h>

// Display
#define DEBUG_SHOW_TOUCH_COORDINATES false

// Boot
#define SHOW_BOOT_INFO true
#define DELAYED_BOOT true
#define DELAYED_BOOT_MS (10000)


// SDcard
#define INIT_SDCARD_ON_BOOT true
#define FORMAT_ON_BOOT false

// HID Keyboard
#define INIT_HID_ON_BOOT true

// LVGL
#define INIT_LVGL_ON_BOOT true

/**
 * @brief LVGL porting example
 * Set the rotation degree:
 *      - 0: 0 degree
 *      - 90: 90 degree
 *      - 180: 180 degree
 *      - 270: 270 degree
 *
 */
#define LVGL_PORT_ROTATION_DEGREE (0)

#endif