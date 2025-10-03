#ifndef USB_SERVICE_H
#define USB_SERVICE_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"

#pragma once
void usb_service_start(void);

#endif