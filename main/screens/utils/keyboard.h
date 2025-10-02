#ifndef KEYBOARD_H
#define KEYBOARD_H

#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "tusb.h"
#include <class/hid/hid.h>
#include <stdbool.h>
#include <stdint.h>

/*
 * Keyboard Sender (TinyUSB + ESP-IDF)
 *
 * Fornece APIs simples para enviar UMA tecla HID (press + release).
 * Não gera loops de repetição.
 *
 * Requisitos:
 *  - Chamar tusb_init() antes de kb_init()
 *  - Alguma parte do código deve chamar tud_task() periodicamente
 *    (ou usar kb_service_usb_for() para um processamento mínimo).
 *
 * Códigos de erro adicionais (faixa arbitrária).
 */
#define KB_ERR_BASE 0x70000
#define KB_ERR_NOT_MOUNTED (KB_ERR_BASE + 1)
#define KB_ERR_NOT_READY (KB_ERR_BASE + 2)
#define KB_ERR_INVALID_PARAM (KB_ERR_BASE + 3)
#define KB_ERR_TIMEOUT (KB_ERR_BASE + 4)

/**
 * Inicializa o módulo (cria mutex interno). Opcionalmente espera o dispositivo montar.
 *
 * @param wait_mount_timeout_ms  Tempo máximo para esperar tud_mounted() (0 = não espera).
 * @return ESP_OK ou erro.
 */
esp_err_t kb_init(uint32_t wait_mount_timeout_ms);

/**
 * Envia uma tecla única (press + release).
 *
 * @param keycode              HID keycode (ex: HID_KEY_A).
 * @param modifier             Máscara de modificadores (ex: KEYBOARD_MODIFIER_LEFT_SHIFT) ou 0.
 * @param press_time_ms        Tempo (ms) a manter a tecla "pressionada" antes do release.
 * @param interface_timeout_ms Tempo máximo para esperar tud_hid_ready() (0 = sem espera).
 *
 * @return ESP_OK em sucesso
 *         KB_ERR_INVALID_PARAM para keycode=0
 *         KB_ERR_NOT_MOUNTED se não montado
 *         KB_ERR_NOT_READY se interface HID não ficou pronta
 *         KB_ERR_TIMEOUT se mutex/esperas estourarem
 */
esp_err_t kb_send_key(uint8_t keycode,
                      uint8_t modifier,
                      uint32_t press_time_ms,
                      uint32_t interface_timeout_ms);

/**
 * Processa o stack USB por uma janela de tempo (em ms).
 * Útil se você não tem uma task dedicada chamando tud_task().
 *
 * @param duration_ms   Tempo total de serviço.
 * @param slice_ms      Quantum por iteração (ex: 5). Se 0, usa 5.
 */
void kb_service_usb_for(uint32_t duration_ms, uint32_t slice_ms);

#endif