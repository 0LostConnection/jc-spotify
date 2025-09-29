#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>
#include <sdcard_utils.h>>

#define WIFI_SSID "."
#define WIFI_PASS "VQdaj5xXxz"
#define WIFI_MAX_RETRY 5

static const char *TAG = "wifi_connect";
static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentando reconectar ao Wi-Fi...");
        } else {
            ESP_LOGI(TAG, "Falha ao conectar ao Wi-Fi.");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Conectado, IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
    }
}

void wifi_connect(void) {
    // Inicializa NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_connect finalizado.");
}

static const char *TAG_HTTP = "http_server";

// Handler para a rota "/"
esp_err_t root_get_handler(httpd_req_t *req) {
    const char resp[] = "Servidor HTTP ESP32 funcionando!";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t keybinds_post_handler(httpd_req_t *req) {
    char buf[1024];
    int total_len = req->content_len;
    int received = 0;

    if (total_len >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Payload muito grande");
        return ESP_FAIL;
    }

    while (received < total_len) {
        int ret = httpd_req_recv(req, buf + received, total_len - received);
        if (ret <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Erro ao receber dados");
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';

    // Salva o CSV recebido no SD card
    if (sdcard_write_file("/sdcard/keybinds.csv", buf)) {
        httpd_resp_sendstr(req, "Keybinds atualizados com sucesso!");
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Falha ao salvar keybinds");
        return ESP_FAIL;
    }
}

esp_err_t keybinds_get_handler(httpd_req_t *req) {
    char buf[2048];
    if (sdcard_read_file("/sdcard/keybinds.csv", buf, sizeof(buf))) {
        httpd_resp_set_type(req, "text/csv");
        httpd_resp_sendstr(req, buf);
        return ESP_OK;
    } else {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Arquivo de keybinds não encontrado");
        return ESP_FAIL;
    }
}

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_post = {
            .uri      = "/keybinds",
            .method   = HTTP_POST,
            .handler  = keybinds_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);

        httpd_uri_t uri_get_keybinds = {
            .uri      = "/keybinds",
            .method   = HTTP_GET,
            .handler  = keybinds_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get_keybinds);

        ESP_LOGI(TAG_HTTP, "Servidor HTTP iniciado!");
    } else {
        ESP_LOGE(TAG_HTTP, "Falha ao iniciar o servidor HTTP");
    }
}

void web_server_init(void) {
    wifi_connect();
    start_webserver();
}
