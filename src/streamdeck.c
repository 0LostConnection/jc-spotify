/**
 * Implementação simples de um layout "Stream Deck" com LVGL.
 *
 * Estratégia:
 * - Um container root usando flex row wrap (5 colunas x 3 linhas) para 320x480 (rotacionado 90 => 480x320).
 * - Cada botão possui estilo de borda, label central e (futuramente) imagem opcional.
 * - Suporte a long press (>= 600 ms) e clique normal.
 * - API para atualizar/alterar teclas.
 */

#include "streamdeck.h"
#include "esp_log.h"

static const char *TAG = "STREAMDECK";

typedef struct {
    sd_key_def_t def;       /* Cópia atual da definição */
    lv_obj_t    *btn;       /* Objeto LVGL do botão */
    lv_obj_t    *label;     /* Label interno */
    uint32_t     press_time;/* Timestamp (lv_tick_get) quando pressionou */
    bool         long_fired;/* Se callback long já foi disparado */
} sd_key_slot_t;

static lv_obj_t *s_root = NULL;
static sd_key_slot_t s_keys[SD_MAX_KEYS];

/* Protótipos internos */
static void key_event_cb(lv_event_t *e);
static void apply_key_definition(sd_key_slot_t *slot);

lv_obj_t *streamdeck_get_container(void) { return s_root; }

void streamdeck_init(void)
{
    if (s_root) {
        ESP_LOGW(TAG, "streamdeck_init já chamado");
        return;
    }

    /* Obtém tela ativa */
    lv_obj_t *scr = lv_scr_act();
    
    s_root = lv_obj_create(scr);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(s_root, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(s_root, 6, 0);
    lv_obj_set_style_pad_row(s_root, 8, 0);
    lv_obj_set_style_pad_column(s_root, 8, 0);
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);

    /* Calcula dimensões base: usamos flex grow, mas fixar garante alinhamento uniforme */
    /* Layout alvo: 5 colunas => cada ~ (100% - gaps)/5 */
    int gap = 8; /* mesmo que pad_column */
    int total_w = lv_obj_get_content_width(scr); /* pode retornar 0 aqui antes de primeira refresh */
    (void)total_w; /* não usado diretamente agora */

    for (int i = 0; i < SD_MAX_KEYS; ++i) {
        sd_key_slot_t *slot = &s_keys[i];
        slot->def.label = NULL;
        slot->def.img_src = NULL;
        slot->def.on_press = NULL;
        slot->def.on_long = NULL;
        slot->def.user_data = NULL;
        slot->press_time = 0;
        slot->long_fired = false;

        lv_obj_t *btn = lv_btn_create(s_root);
        slot->btn = btn;
        lv_obj_set_size(btn, (480 - (gap * (SD_COLS - 1)) - 12) / SD_COLS, (320 - (gap * (SD_ROWS - 1)) - 12) / SD_ROWS);
        lv_obj_set_style_radius(btn, 14, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x202020), 0);
        lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x404040), 0);
        lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x606060), 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(btn, key_event_cb, LV_EVENT_ALL, (void *)(intptr_t)i);

        lv_obj_t *label = lv_label_create(btn);
        slot->label = label;
        lv_label_set_text(label, "");
        lv_obj_center(label);
    }

    ESP_LOGI(TAG, "Stream Deck UI inicializado (%dx%d)", SD_COLS, SD_ROWS);

    /* Exemplo: preenche as 3 primeiras teclas com ações de demonstração */
    sd_key_def_t demo1 = { .label = "LED+", .on_press = NULL, .on_long = NULL, .user_data = NULL };
    sd_key_def_t demo2 = { .label = "LED-", .on_press = NULL, .on_long = NULL, .user_data = NULL };
    sd_key_def_t demo3 = { .label = "INFO", .on_press = NULL, .on_long = NULL, .user_data = NULL };
    streamdeck_set_key(0, &demo1);
    streamdeck_set_key(1, &demo2);
    streamdeck_set_key(2, &demo3);
}

static void apply_key_definition(sd_key_slot_t *slot)
{
    if (!slot || !slot->btn) return;
    /* Atualiza label */
    if (slot->label) {
        lv_label_set_text(slot->label, slot->def.label ? slot->def.label : "");
    }
    /* TODO: suporte a ícone futuro (pode criar lv_img como child) */
}

bool streamdeck_set_key(uint8_t index, const sd_key_def_t *def)
{
    if (index >= SD_MAX_KEYS || !def) return false;
    sd_key_slot_t *slot = &s_keys[index];
    slot->def = *def; /* cópia rasa */
    apply_key_definition(slot);
    return true;
}

bool streamdeck_set_key_label(uint8_t index, const char *label)
{
    if (index >= SD_MAX_KEYS) return false;
    s_keys[index].def.label = label;
    apply_key_definition(&s_keys[index]);
    return true;
}

bool streamdeck_bind_action(uint8_t index, sd_action_cb on_press, sd_longpress_cb on_long, void *user_data)
{
    if (index >= SD_MAX_KEYS) return false;
    s_keys[index].def.on_press = on_press;
    s_keys[index].def.on_long  = on_long;
    s_keys[index].def.user_data = user_data;
    return true;
}

void streamdeck_reset_all(void)
{
    for (int i=0;i<SD_MAX_KEYS;i++) {
        s_keys[i].def.label = NULL;
        s_keys[i].def.img_src = NULL;
        s_keys[i].def.on_press = NULL;
        s_keys[i].def.on_long = NULL;
        s_keys[i].def.user_data = NULL;
        if (s_keys[i].label) lv_label_set_text(s_keys[i].label, "");
    }
}

void streamdeck_iterate(streamdeck_iter_cb cb)
{
    if (!cb) return;
    for (uint8_t i=0;i<SD_MAX_KEYS;i++) {
        void **slot_ud = &s_keys[i].def.user_data;
        if (!cb(i, slot_ud)) break;
    }
}

static void key_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t index = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (index >= SD_MAX_KEYS) return;
    sd_key_slot_t *slot = &s_keys[index];

    switch (code) {
    case LV_EVENT_PRESSED:
        slot->press_time = lv_tick_get();
        slot->long_fired = false;
        /* feedback visual */
        lv_obj_set_style_bg_color(slot->btn, lv_color_hex(0x0080FF), LV_PART_MAIN | LV_STATE_DEFAULT);
        break;
    case LV_EVENT_PRESSING:
        if (!slot->long_fired && slot->def.on_long) {
            uint32_t elapsed = lv_tick_elaps(slot->press_time);
            if (elapsed > 600) { /* long press limiar */
                slot->long_fired = true;
                slot->def.on_long(slot->def.user_data);
            }
        }
        break;
    case LV_EVENT_RELEASED:
        /* restaura cor */
        lv_obj_set_style_bg_color(slot->btn, lv_color_hex(0x202020), LV_PART_MAIN | LV_STATE_DEFAULT);
        if (!slot->long_fired && slot->def.on_press) {
            slot->def.on_press(slot->def.user_data);
        }
        break;
    case LV_EVENT_PRESS_LOST:
    case LV_EVENT_DELETE:
        lv_obj_set_style_bg_color(slot->btn, lv_color_hex(0x202020), LV_PART_MAIN | LV_STATE_DEFAULT);
        break;
    default:
        break;
    }
}
