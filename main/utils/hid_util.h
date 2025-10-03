#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hid_util_init(void);

// Função para enviar um atalho de teclado (ex: CTRL+ALT+DEL, WIN+L, etc)
bool hid_send_shortcut(uint8_t modifiers, const uint8_t *keycodes, uint8_t keycount);

// Função para enviar teclas multimídia (ex: volume, play/pause, etc)
bool hid_send_consumer_key(uint16_t usage_id);

#ifdef __cplusplus
}
#endif