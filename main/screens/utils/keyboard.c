#include "keyboard.h"
#include "tusb.h"

static const char *TAG = "KB_SENDER";
static SemaphoreHandle_t s_mutex = NULL;

static esp_err_t kb_lock(TickType_t ticks) {
    if (xSemaphoreTake(s_mutex, ticks) != pdTRUE) {
        return KB_ERR_TIMEOUT;
    }
    return ESP_OK;
}

static void kb_unlock(void) {
    xSemaphoreGive(s_mutex);
}

esp_err_t kb_init(uint32_t wait_mount_timeout_ms) {
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (!s_mutex) {
            ESP_LOGE(TAG, "Falha ao criar mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (wait_mount_timeout_ms > 0) {
        uint32_t waited = 0;
        const uint32_t step = 10;
        while (!tud_mounted() && waited < wait_mount_timeout_ms) {
            tud_task();
            vTaskDelay(pdMS_TO_TICKS(step));
            waited += step;
        }
    }

    return ESP_OK;
}

esp_err_t kb_send_key(uint8_t keycode,
                      uint8_t modifier,
                      uint32_t press_time_ms,
                      uint32_t interface_timeout_ms) {
    if (keycode == 0) {
        return KB_ERR_INVALID_PARAM;
    }

    if (!tud_mounted()) {
        return KB_ERR_NOT_MOUNTED;
    }

    // Espera HID pronta
    if (!tud_hid_ready()) {
        uint32_t waited = 0;
        const uint32_t step = 5;
        while (!tud_hid_ready() &&
               (interface_timeout_ms == 0 || waited < interface_timeout_ms)) {
            tud_task();
            vTaskDelay(pdMS_TO_TICKS(step));
            waited += step;
        }
        if (!tud_hid_ready()) {
            return KB_ERR_NOT_READY;
        }
    }

    esp_err_t err = kb_lock(pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        return err;
    }

    // Relatório com uma tecla
    uint8_t report[6] = {0};
    report[0] = keycode;

    bool pressed_ok = tud_hid_keyboard_report(0, modifier, report);
    if (!pressed_ok) {
        kb_unlock();
        return KB_ERR_NOT_READY;
    }

    // Mantém pressionado pelo tempo desejado
    if (press_time_ms) {
        uint32_t elapsed = 0;
        const uint32_t slice = 5;
        while (elapsed < press_time_ms) {
            vTaskDelay(pdMS_TO_TICKS(slice));
            tud_task();
            elapsed += slice;
        }
    } else {
        // Pequeno atraso mínimo para host registrar
        vTaskDelay(pdMS_TO_TICKS(10));
        tud_task();
    }

    // Release
    tud_hid_keyboard_report(0, 0, NULL);

    // Serviço extra breve para garantir envio do release
    vTaskDelay(pdMS_TO_TICKS(5));
    tud_task();

    kb_unlock();
    return ESP_OK;
}

void kb_service_usb_for(uint32_t duration_ms, uint32_t slice_ms) {
    if (slice_ms == 0)
        slice_ms = 5;
    uint32_t elapsed = 0;
    while (elapsed < duration_ms) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(slice_ms));
        elapsed += slice_ms;
    }
}

/* Callbacks HID mínimos (pode manter assim se não precisar de GET_REPORT/SET_REPORT) */
uint16_t tud_hid_get_report_cb(uint8_t instance,
                               uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer,
                               uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance,
                           uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer,
                           uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
    // Não tratado neste módulo.
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
}

void tud_resume_cb(void) {
}