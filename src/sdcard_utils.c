#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <dirent.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#define SD_MMC_D0   13
#define SD_MMC_CLK  12
#define SD_MMC_CMD  11

static const char *TAG = "SDCARD";

void sdcard_init(void) {
    esp_err_t ret;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_1;
    host.flags = SDMMC_HOST_FLAG_1BIT; // 1-bit mode

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    slot_config.clk = SD_MMC_CLK;
    slot_config.cmd = SD_MMC_CMD;
    slot_config.d0 = SD_MMC_D0;

    gpio_set_pull_mode(SD_MMC_CMD, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_MMC_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_MMC_D0, GPIO_PULLUP_ONLY);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD card montado com sucesso!");
    } else {
        ESP_LOGE(TAG, "Falha ao montar SD card (%s)", esp_err_to_name(ret));
        return;
    }

    // Exemplo: listar arquivos no diretório raiz
    DIR* dir = opendir("/sdcard");
    if (dir) {
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            ESP_LOGI(TAG, "Arquivo: %s", ent->d_name);
        }
        closedir(dir);
    }
}

void sdcard_list_files(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) {
        ESP_LOGE(TAG, "Não foi possível abrir o diretório: %s", path);
        return;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "Arquivo: %s", ent->d_name);
    }
    closedir(dir);
}

bool sdcard_read_file(const char *path, char *buffer, size_t max_len) {
    FILE *f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para leitura: %s", path);
        return false;
    }
    size_t read = fread(buffer, 1, max_len - 1, f);
    buffer[read] = '\0';
    fclose(f);
    ESP_LOGI(TAG, "Arquivo lido: %s (%d bytes)", path, (int)read);
    return true;
}

bool sdcard_write_file(const char *path, const char *data) {
    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Falha ao abrir arquivo para escrita: %s", path);
        return false;
    }
    size_t written = fwrite(data, 1, strlen(data), f);
    fclose(f);
    ESP_LOGI(TAG, "Arquivo escrito: %s (%d bytes)", path, (int)written);
    return written == strlen(data);
}

bool sdcard_delete_file(const char *path) {
    int res = remove(path);
    if (res == 0) {
        ESP_LOGI(TAG, "Arquivo deletado: %s", path);
        return true;
    } else {
        ESP_LOGE(TAG, "Falha ao deletar arquivo: %s", path);
        return false;
    }
}