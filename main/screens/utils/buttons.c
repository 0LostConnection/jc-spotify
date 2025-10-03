#include "buttons.h"
#include "styles.h"
#include <esp/display.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Onde o arquivo CSV será armazenado
#define CSV_FILE_PATH "/sdcard/buttons.csv"
#define MAX_BUTTONS 9

// --- Funções Auxiliares ---

// Converte a string do tipo de ação para o enum
ACTION_TYPE get_action_type_from_string(const char *type_str) {
    if (strcmp(type_str, "SCREEN") == 0)
        return SCREEN;
    if (strcmp(type_str, "SYSTEM") == 0)
        return SYSTEM;
    if (strcmp(type_str, "SHORTCUT") == 0)
        return SHORTCUT;
    // Retorna SHORTCUT como padrão ou um valor de erro se for o caso
    return SHORTCUT;
}

// Cria um arquivo CSV padrão no SD Card com configurações iniciais
void create_default_csv() {
    FILE *f = fopen(CSV_FILE_PATH, "w");
    if (f == NULL) {
        printf("ERRO: Nao foi possivel criar o arquivo CSV em %s\n", CSV_FILE_PATH);
        return;
    }

    // Estrutura do CSV: label;type;action
    fprintf(f, "C;SCREEN;CONFIG\n");
    fprintf(f, "Mute;SHORTCUT;MUTE\n");
    fprintf(f, "VolUp;SHORTCUT;VOLUME_UP\n");
    fprintf(f, "VolDown;SHORTCUT;VOLUME_DOWN\n");
    fprintf(f, "Play;SHORTCUT;PLAY_PAUSE\n");
    fprintf(f, "Undo;SHORTCUT;CTRL_Z\n");
    fprintf(f, "Copy;SHORTCUT;CTRL_C\n");
    fprintf(f, "Paste;SHORTCUT;CTRL_V\n");
    fprintf(f, "SlctAll;SHORTCUT;CTRL_A\n");

    fclose(f);
    printf("SUCESSO: Arquivo CSV padrao criado em %s\n", CSV_FILE_PATH);
}

// **Substitui a função get_button_actions**
int read_button_actions_from_csv(bnt_action buttons[]) {
    FILE *f = fopen(CSV_FILE_PATH, "r");
    if (f == NULL) {
        printf("AVISO: Arquivo CSV nao encontrado. Criando padrao...\n");
        create_default_csv();
        // Tenta abrir o arquivo recém-criado
        f = fopen(CSV_FILE_PATH, "r");
        if (f == NULL) {
            printf("ERRO FATAL: Nao foi possivel abrir o arquivo padrao.\n");
            return 0;
        }
    }

    char line[200];
    int count = 0;

    // Loop para ler linha por linha
    while (fgets(line, sizeof(line), f) != NULL && count < MAX_BUTTONS) {
        // Remove quebras de linha
        line[strcspn(line, "\r\n")] = 0;

        // Se a linha for vazia, continua para a próxima
        if (strlen(line) == 0)
            continue;

        // Tokeniza a linha usando o delimitador ';'
        char *token;
        char *rest = line;
        char fields[3][50]; // Array temporário para armazenar os 3 campos

        // Extrai os 3 campos (label, type, action)
        for (int i = 0; i < 3; i++) {
            token = strtok_r(rest, ";", &rest);
            if (token != NULL) {
                // Copia o campo e garante que nao exceda o buffer
                strncpy(fields[i], token, 49);
                fields[i][49] = '\0';
            } else {
                // Linha incompleta, ignora
                printf("AVISO: Linha incompleta no CSV, ignorando...\n");
                goto next_line;
            }
        }

        // Preenche a struct bnt_action com os dados lidos
        strncpy(buttons[count].label, fields[0], 49);
        buttons[count].type = get_action_type_from_string(fields[1]);
        strncpy(buttons[count].action, fields[2], 49);

        count++;
    next_line:;
    }

    fclose(f);
    printf("SUCESSO: %d botoes carregados do CSV.\n", count);
    return count;
}

lv_obj_t *create_button(lv_obj_t *parent, lv_event_cb_t callback_function, bnt_action *action_data, lv_coord_t width, lv_coord_t height) {
    init_styles();

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, width, height);
    lv_obj_add_style(btn, &style_buttons, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, action_data->label);
    lv_obj_center(label);

    lv_obj_set_user_data(btn, (void *)action_data);

    lv_obj_add_event_cb(btn, callback_function, LV_EVENT_CLICKED, NULL);

    return btn;
}