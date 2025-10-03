#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SD_MMC_D0 13
#define SD_MMC_CLK 12
#define SD_MMC_CMD 11

#define SD_MOUNT_POINT "/sdcard"
#define SD_UTIL_MAX_FILES 4
#define SD_UTIL_MAX_FILENAME 64

typedef struct {
    char name[SD_UTIL_MAX_FILENAME];
} sd_util_file_info_t;

typedef struct {
    sd_util_file_info_t files[SD_UTIL_MAX_FILES];
    int count;
} sd_util_file_list_t;

// Inicializa o SD. Retorna ESP_OK em caso de sucesso.
esp_err_t sd_util_init(void);

// Destroi (desmonta) o SD, liberando recursos.
void sd_util_deinit(void);

// Grava uma string em um arquivo no SD.
esp_err_t sd_util_write_file(const char *path, const char *content);

// Lê uma linha de um arquivo (até 255 chars) para o buffer. Retorna ESP_OK em caso de sucesso.
esp_err_t sd_util_read_file(const char *path, char *buffer, size_t max_len);

// preenche sd_util_file_list_t com os nomes dos arquivos do diretório
esp_err_t sd_util_list_dir(const char *dir_path, sd_util_file_list_t *file_list);
#ifdef __cplusplus
}
#endif