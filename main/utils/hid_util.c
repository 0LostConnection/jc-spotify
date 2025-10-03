#include "hid_util.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tusb.h"

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
        tud_hid_report(0, (uint8_t *)&usage_id, sizeof(usage_id));
        vTaskDelay(pdMS_TO_TICKS(30));
        uint16_t zero = 0;
        tud_hid_report(0, (uint8_t *)&zero, sizeof(zero));
        xSemaphoreGive(hid_mutex);
        return true;
    } else {
        ESP_LOGW(TAG, "HID mutex timeout");
        return false;
    }
}