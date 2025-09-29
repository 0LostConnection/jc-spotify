/**
 * Simple LVGL "Stream Deck" style UI scaffolding for the JC3248W535 board.
 *
 * Objetivo: fornecer uma base fácil para você criar teclas (botões) dinâmicas
 * que disparam actions/callbacks e podem ter rótulo e/ou ícone.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SD_MAX_KEYS  15          /* 5 x 3 layout */
#define SD_COLS       5
#define SD_ROWS       3

typedef void (*sd_action_cb)(void *user_data);          /* Callback para clique curto */
typedef void (*sd_longpress_cb)(void *user_data);       /* Callback para long press */

typedef struct {
    const char      *label;      /* Texto do botão (opcional) */
    const void      *img_src;    /* Fonte de imagem LVGL (opcional: &img_xxx ou caminho FS se habilitado) */
    sd_action_cb     on_press;   /* Ação ao pressionar */
    sd_longpress_cb  on_long;    /* Ação ao segurar */
    void            *user_data;  /* Contexto do usuário */
} sd_key_def_t;

/**
 * Inicializa a tela principal do Stream Deck (substitui demos). Cria container e botões vazios.
 */
void streamdeck_init(void);

/**
 * Define/atualiza a definição de uma tecla (por índice linear 0..SD_MAX_KEYS-1)
 * Retorna false se índice inválido.
 */
bool streamdeck_set_key(uint8_t index, const sd_key_def_t *def);

/**
 * Atualiza dinamicamente apenas o label de uma key (atalho conveniência).
 */
bool streamdeck_set_key_label(uint8_t index, const char *label);

/**
 * Retorna objeto lvgl principal (container) para customizações adicionais.
 */
lv_obj_t *streamdeck_get_container(void);

/**
 * Faz bind de callbacks (press/long) para uma key existente sem alterar label/imagem.
 */
bool streamdeck_bind_action(uint8_t index, sd_action_cb on_press, sd_longpress_cb on_long, void *user_data);

/**
 * Limpa todas as teclas (remove labels e callbacks).
 */
void streamdeck_reset_all(void);

/* Iterador sobre teclas para pós-processamento.
 * Chama cb(index, user_data_ptr) para cada slot; se cb retornar false interrompe.
 */
typedef bool (*streamdeck_iter_cb)(uint8_t index, void **user_data_slot);
void streamdeck_iterate(streamdeck_iter_cb cb);


#ifdef __cplusplus
}
#endif
