#ifndef KEYBINDS_H
#define KEYBINDS_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char label[32];
    char actionType[16];
    char action[64];
} keybind_t;

bool keybinds_load(const char *path, keybind_t *keybinds, size_t max_keybinds, size_t *count);
bool keybinds_save(const char *path, keybind_t *keybinds, size_t count);

#ifdef __cplusplus
}
#endif

#endif // KEYBINDS_H