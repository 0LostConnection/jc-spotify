#include "usb_service.h"

static void usb_service_task(void *arg) {
    (void)arg;
    while (1) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void usb_service_start(void) {
    xTaskCreatePinnedToCore(usb_service_task, "usb_svc", 4096, NULL, 6, NULL, 0);
}