#include "shortcut_handler.h"
// Você pode incluir "class/hid/hid.h" se precisar de definições extras:

#define SHORTCUT_MAX_LEN 48

typedef struct {
    char text[SHORTCUT_MAX_LEN];
} shortcut_evt_t;

static QueueHandle_t s_queue = NULL;
static bool s_debug = false;
static const char *TAG = "SHORTCUT";
static uint32_t s_press_time_ms = 40;
static uint32_t s_interface_wait_ms = 500;

/* Mapeamento básico de tokens para keycodes HID */
typedef struct {
    const char *name;
    uint8_t keycode;
} key_token_t;

static const key_token_t s_key_tokens[] = {
    {"A", HID_KEY_A}, {"B", HID_KEY_B}, {"C", HID_KEY_C}, {"D", HID_KEY_D},
    {"E", HID_KEY_E}, {"F", HID_KEY_F}, {"G", HID_KEY_G}, {"H", HID_KEY_H},
    {"I", HID_KEY_I}, {"J", HID_KEY_J}, {"K", HID_KEY_K}, {"L", HID_KEY_L},
    {"M", HID_KEY_M}, {"N", HID_KEY_N}, {"O", HID_KEY_O}, {"P", HID_KEY_P},
    {"Q", HID_KEY_Q}, {"R", HID_KEY_R}, {"S", HID_KEY_S}, {"T", HID_KEY_T},
    {"U", HID_KEY_U}, {"V", HID_KEY_V}, {"W", HID_KEY_W}, {"X", HID_KEY_X},
    {"Y", HID_KEY_Y}, {"Z", HID_KEY_Z},
    {"1", HID_KEY_1}, {"2", HID_KEY_2}, {"3", HID_KEY_3}, {"4", HID_KEY_4},
    {"5", HID_KEY_5}, {"6", HID_KEY_6}, {"7", HID_KEY_7}, {"8", HID_KEY_8},
    {"9", HID_KEY_9}, {"0", HID_KEY_0},
    {"ENTER", HID_KEY_ENTER},
    {"ESC", HID_KEY_ESCAPE}, {"ESCAPE", HID_KEY_ESCAPE},
    {"TAB", HID_KEY_TAB},
    {"SPACE", HID_KEY_SPACE},
    {"MINUS", HID_KEY_MINUS}, {"EQUAL", HID_KEY_EQUAL},
    {"LEFT", HID_KEY_ARROW_LEFT},
    {"RIGHT", HID_KEY_ARROW_RIGHT},
    {"UP", HID_KEY_ARROW_UP},
    {"DOWN", HID_KEY_ARROW_DOWN},
    {"DEL", HID_KEY_DELETE}, {"DELETE", HID_KEY_DELETE},
    {"BACKSPACE", HID_KEY_BACKSPACE},
    {"HOME", HID_KEY_HOME},
    {"END", HID_KEY_END},
    {"PAGEUP", HID_KEY_PAGE_UP},
    {"PAGEDOWN", HID_KEY_PAGE_DOWN},
    {"F1", HID_KEY_F1}, {"F2", HID_KEY_F2}, {"F3", HID_KEY_F3}, {"F4", HID_KEY_F4},
    {"F5", HID_KEY_F5}, {"F6", HID_KEY_F6}, {"F7", HID_KEY_F7}, {"F8", HID_KEY_F8},
    {"F9", HID_KEY_F9}, {"F10", HID_KEY_F10}, {"F11", HID_KEY_F11}, {"F12", HID_KEY_F12},
    {"PAUSE", HID_KEY_PAUSE},
    {"PRINTSCREEN", HID_KEY_PRINT_SCREEN},
    {"INSERT", HID_KEY_INSERT},
    {NULL, 0}
};

/* Normaliza string em-lugar: remove espaços e converte para upper */
static void normalize_token(char *s) {
    char *dst = s;
    for (char *p = s; *p; ++p) {
        if (*p == ' ') continue;
        *dst++ = (char)toupper((unsigned char)*p);
    }
    *dst = 0;
}

static uint8_t find_keycode(const char *token) {
    for (int i = 0; s_key_tokens[i].name; i++) {
        if (strcmp(s_key_tokens[i].name, token) == 0) {
            return s_key_tokens[i].keycode;
        }
    }
    return 0;
}

/* Analisa o atalho, produz keycode final + modifiers */
static bool parse_shortcut(const char *shortcut, uint8_t *out_key, uint8_t *out_mod) {
    if (!shortcut || !out_key || !out_mod) return false;
    *out_key = 0;
    *out_mod = 0;

    // Copia para buffer mutável
    char buf[SHORTCUT_MAX_LEN];
    strncpy(buf, shortcut, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    // Substitui caracteres '+' por separadores e processa
    // Estratégia: tokenizar por '+'
    char *saveptr = NULL;
    char *token = strtok_r(buf, "+", &saveptr);

    int key_tokens = 0;
    while (token) {
        normalize_token(token);

        if (strcmp(token, "CTRL") == 0 || strcmp(token, "CONTROL") == 0) {
            *out_mod |= HID_KEY_CONTROL_LEFT;
        } else if (strcmp(token, "SHIFT") == 0) {
            *out_mod |= HID_KEY_SHIFT_LEFT;
        } else if (strcmp(token, "ALT") == 0) {
            *out_mod |= HID_KEY_ALT_LEFT;
        } else if (strcmp(token, "GUI") == 0 || strcmp(token, "WIN") == 0 ||
                   strcmp(token, "CMD") == 0 || strcmp(token, "SUPER") == 0) {
            *out_mod |= HID_KEY_GUI_LEFT;
        } else {
            // Assume que é a tecla principal
            uint8_t kc = find_keycode(token);
            if (kc == 0) {
                if (s_debug) {
                    ESP_LOGW(TAG, "Token desconhecido: %s", token);
                }
                return false;
            }
            *out_key = kc;
            key_tokens++;
        }

        token = strtok_r(NULL, "+", &saveptr);
    }

    if (*out_key == 0) {
        // Não foi especificada tecla principal
        return false;
    }
    (void)key_tokens;
    return true;
}

static void shortcut_worker_task(void *arg) {
    (void)arg;
    shortcut_evt_t evt;
    while (1) {
        if (xQueueReceive(s_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (s_debug) {
                ESP_LOGI(TAG, "Processando atalho: '%s'", evt.text);
            }
            uint8_t keycode = 0, modifier = 0;
            if (!parse_shortcut(evt.text, &keycode, &modifier)) {
                ESP_LOGW(TAG, "Falha no parse do atalho: %s", evt.text);
                continue;
            }
            esp_err_t r = kb_send_key(keycode, modifier, s_press_time_ms, s_interface_wait_ms);
            if (r != ESP_OK) {
                ESP_LOGW(TAG, "kb_send_key falhou (0x%x) para '%s'", r, evt.text);
            } else if (s_debug) {
                ESP_LOGI(TAG, "Atalho '%s' enviado (kc=0x%02X mod=0x%02X)", evt.text, keycode, modifier);
            }
        }
    }
}

esp_err_t shortcut_handler_init(size_t max_queue) {
    if (s_queue) return ESP_OK;
    if (max_queue == 0) max_queue = 8;
    s_queue = xQueueCreate(max_queue, sizeof(shortcut_evt_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    BaseType_t ok = xTaskCreatePinnedToCore(
        shortcut_worker_task,
        "shortcut_worker",
        4096,
        NULL,
        5,
        NULL,
        0
    );
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t shortcut_enqueue(const char *shortcut_str, TickType_t timeout_ticks) {
    if (!s_queue || !shortcut_str) return ESP_ERR_INVALID_STATE;
    shortcut_evt_t evt;
    size_t len = strnlen(shortcut_str, SHORTCUT_MAX_LEN - 1);
    strncpy(evt.text, shortcut_str, len);
    evt.text[len] = 0;

    if (xQueueSend(s_queue, &evt, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void shortcut_set_timings(uint32_t press_time_ms, uint32_t interface_wait_ms) {
    s_press_time_ms = press_time_ms;
    s_interface_wait_ms = interface_wait_ms;
}

void shortcut_enable_debug(bool en) {
    s_debug = en;
}