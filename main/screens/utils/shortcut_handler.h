#ifndef SHORTCUT_HANDLER_H
#define SHORTCUT_HANDLER_H
#pragma once
#include "class/hid/hid.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "keyboard.h"
#include "tusb.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <portmacro.h>

// Inicializa fila e task de processamento de atalhos.
// max_queue: número máximo de eventos pendentes.
// Retorna ESP_OK ou erro.
esp_err_t shortcut_handler_init(size_t max_queue);

// Enfileira um atalho (string). Copiamos internamente.
// timeout_ticks = ticks para esperar fila ter espaço (0 = não espera).
// Retorna ESP_OK ou ESP_ERR_TIMEOUT.
esp_err_t shortcut_enqueue(const char *shortcut_str, TickType_t timeout_ticks);

// Opcional: ajustar tempo de "press" padrão e timeout de interface
void shortcut_set_timings(uint32_t press_time_ms, uint32_t interface_wait_ms);

// Define se logs de debug do parser ficam ativos
void shortcut_enable_debug(bool en);

#endif