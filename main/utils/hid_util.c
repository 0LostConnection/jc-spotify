#include "hid_util.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

// HID report descriptor: Keyboard + Mouse
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2)),
};

// String descriptor
const char *hid_string_descriptor[5] = {
    (char[]){0x09, 0x04},    // 0: English (0x0409)
    "TinyUSB",               // 1: Manufacturer
    "TinyUSB Device",        // 2: Product
    "123456",                // 3: Serial
    "Example HID interface", // 4: HID
};

// Configuration descriptor
static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

// TinyUSB HID callbacks
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

static const char *TAG = "hid_util";

static SemaphoreHandle_t hid_mutex = NULL;

void hid_util_init(void) {
    if (!hid_mutex) {
        hid_mutex = xSemaphoreCreateMutex();
        if (!hid_mutex) {
            ESP_LOGE(TAG, "Failed to create HID mutex!");
        }
    }
}

void hid_util_setup_usb(void) {
    static const char *TAG = "hid_util";
    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = hid_configuration_descriptor;
#endif
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");
    hid_util_init();
}

// Envia um atalho de teclado (ex: CTRL+ALT+DEL)
// Retorna true se enviado com sucesso, false caso contrário
bool hid_send_shortcut(uint8_t modifiers, const uint8_t *keycodes, uint8_t keycount) {
    if (!hid_mutex || !tud_mounted())
        return false;

    if (xSemaphoreTake(hid_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        uint8_t keys[6] = {0};
        for (uint8_t i = 0; i < keycount && i < 6; i++) {
            keys[i] = keycodes[i];
        }
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifiers, keys);
        vTaskDelay(pdMS_TO_TICKS(30));
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        xSemaphoreGive(hid_mutex);
        return true;
    } else {
        ESP_LOGW(TAG, "HID mutex timeout");
        return false;
    }
}

// Envia teclas multimídia (consumer control), como volume, play, etc
// Retorna true se enviado com sucesso, false caso contrário
bool hid_send_consumer_key(uint16_t usage_id) {
    if (!hid_mutex || !tud_mounted())
        return false;

    if (xSemaphoreTake(hid_mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        tud_hid_report(2, (uint8_t *)&usage_id, sizeof(usage_id));
        vTaskDelay(pdMS_TO_TICKS(30));
        uint16_t zero = 0;
        tud_hid_report(2, (uint8_t *)&zero, sizeof(zero));
        xSemaphoreGive(hid_mutex);
        return true;
    } else {
        ESP_LOGW(TAG, "HID mutex timeout");
        return false;
    }
}