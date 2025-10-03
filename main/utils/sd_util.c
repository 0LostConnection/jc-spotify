#include "sd_util.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "SD_UTIL";
static sdmmc_card_t *sd_card = NULL;

esp_err_t sd_util_init(void) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024};

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

    // Mapear pinos customizados
    slot_config.clk = SD_MMC_CLK;
    slot_config.cmd = SD_MMC_CMD;
    slot_config.d0 = SD_MMC_D0;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &sd_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao montar o cartão SD (%s)", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Cartão SD montado.");
    sdmmc_card_print_info(stdout, sd_card);
    return ESP_OK;
}

void sd_util_deinit(void) {
    if (sd_card) {
        esp_vfs_fat_sdcard_unmount(SD_MOUNT_POINT, sd_card);
        sd_card = NULL;
        ESP_LOGI(TAG, "Cartão SD desmontado.");
    }
}

esp_err_t sd_util_write_file(const char *path, const char *content) {
    char full_path[128];
    snprintf(full_path, sizeof(full_path), SD_MOUNT_POINT "/%s", path);
    FILE *f = fopen(full_path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita: %s", full_path);
        return ESP_FAIL;
    }
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    ESP_LOGI(TAG, "Arquivo escrito: %s", full_path);
    return ESP_OK;
}

esp_err_t sd_util_read_file(const char *path, char *buffer, size_t max_len) {
    char full_path[128];
    snprintf(full_path, sizeof(full_path), SD_MOUNT_POINT "/%s", path);
    FILE *f = fopen(full_path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para leitura: %s", full_path);
        return ESP_FAIL;
    }
    if (fgets(buffer, max_len, f) == NULL) {
        ESP_LOGE(TAG, "Falha ao ler do arquivo: %s", full_path);
        fclose(f);
        return ESP_FAIL;
    }
    fclose(f);
    ESP_LOGI(TAG, "Arquivo lido: %s", full_path);
    return ESP_OK;
}

esp_err_t sd_util_list_dir(const char *dir_path, sd_util_file_list_t *file_list) {
    if (!file_list)
        return ESP_ERR_INVALID_ARG;
    file_list->count = 0;

    char full_path[128];
    snprintf(full_path, sizeof(full_path), SD_MOUNT_POINT "/%s", dir_path ? dir_path : "");

    DIR *dir = opendir(full_path);
    if (!dir) {
        ESP_LOGE(TAG, "Nao foi possivel abrir diretorio: %s", full_path);
        return ESP_FAIL;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && file_list->count < SD_UTIL_MAX_FILES) {
        strncpy(file_list->files[file_list->count].name, entry->d_name, SD_UTIL_MAX_FILENAME - 1);
        file_list->files[file_list->count].name[SD_UTIL_MAX_FILENAME - 1] = '\0';
        file_list->count++;
    }
    closedir(dir);
    return ESP_OK;
}