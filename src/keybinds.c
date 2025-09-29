#include "keybinds.h"
#include "sdcard_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

bool keybinds_load(const char *path, keybind_t *keybinds, size_t max_keybinds, size_t *count) {
    char buffer[256];
    if (!sdcard_read_file(path, buffer, sizeof(buffer))) return false;

    *count = 0;
    char *line = strtok(buffer, "\n");
    while (line && *count < max_keybinds) {
        char *label = strtok(line, ",");
        char *actionType = strtok(NULL, ",");
        char *action = strtok(NULL, ",");

        if (label && actionType && action) {
            strncpy(keybinds[*count].label, label, sizeof(keybinds[*count].label) - 1);
            strncpy(keybinds[*count].actionType, actionType, sizeof(keybinds[*count].actionType) - 1);
            strncpy(keybinds[*count].action, action, sizeof(keybinds[*count].action) - 1);
            (*count)++;
        }
        line = strtok(NULL, "\n");
    }
    return true;
}

bool keybinds_save(const char *path, keybind_t *keybinds, size_t count) {
    char buffer[1024] = {0};
    for (size_t i = 0; i < count; i++) {
        char line[128];
        snprintf(line, sizeof(line), "%s,%s,%s\n",
                 keybinds[i].label, keybinds[i].actionType, keybinds[i].action);
        strncat(buffer, line, sizeof(buffer) - strlen(buffer) - 1);
    }
    return sdcard_write_file(path, buffer);
}