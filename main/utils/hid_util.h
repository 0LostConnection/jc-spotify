#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hid_util_init(void);

bool hid_send_shortcut(uint8_t modifiers, const uint8_t *keycodes, uint8_t keycount);
bool hid_send_consumer_key(uint16_t usage_id);

#ifdef __cplusplus
}
#endif