/**
 * @file ec_session.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

/* ==================== [Includes] ========================================== */

#include "ec_session.h"
#include "ec_config_internal.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dirent.h>
#include <time.h>
#include "esp_log.h"
#include "cJSON.h"

/* ==================== [Defines] =========================================== */

#define EC_SESSION_PATH_MAX 128
#define EC_SESSION_LINE_MAX 4096

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static uint32_t chat_id_hash(const char *s);
static void session_path(const char *chat_id, char *buf, size_t size);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "session";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_session_append(const char *chat_id, const char *role, const char *content)
{
    char path[EC_SESSION_PATH_MAX];
    session_path(chat_id, path, sizeof(path));

    FILE *f = fopen(path, "a");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open session file %s", path);
        return ESP_FAIL;
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "role", role);
    cJSON_AddStringToObject(obj, "content", content);
    cJSON_AddNumberToObject(obj, "ts", (double)time(NULL));

    char *line = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    if (line) {
        fprintf(f, "%s\n", line);
        free(line);
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t ec_session_append_msg(const char *chat_id, const cJSON *msg)
{
    if (!chat_id || !msg) {
        return ESP_ERR_INVALID_ARG;
    }

    char path[EC_SESSION_PATH_MAX];
    session_path(chat_id, path, sizeof(path));

    FILE *f = fopen(path, "a");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open session file %s", path);
        return ESP_FAIL;
    }

    cJSON *copy = cJSON_Duplicate(msg, 1);
    if (!copy) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddNumberToObject(copy, "ts", (double)time(NULL));

    char *line = cJSON_PrintUnformatted(copy);
    cJSON_Delete(copy);

    if (line) {
        fprintf(f, "%s\n", line);
        free(line);
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t ec_session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int history_limit = max_msgs;
    if (history_limit <= 0) {
        ESP_LOGW(TAG, "Invalid max_msgs=%d, clamping to 1", max_msgs);
        history_limit = 1;
    } else if (history_limit > EC_SESSION_MAX_MSGS) {
        ESP_LOGW(TAG, "max_msgs=%d exceeds limit %d, clamping",
                 max_msgs, EC_SESSION_MAX_MSGS);
        history_limit = EC_SESSION_MAX_MSGS;
    }

    char path[EC_SESSION_PATH_MAX];
    session_path(chat_id, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        /* No history yet */
        snprintf(buf, size, "[]");
        return ESP_OK;
    }

    /* Read all lines into a ring buffer of cJSON objects */
    cJSON *messages[EC_SESSION_MAX_MSGS] = {0};
    int count = 0;
    int write_idx = 0;

    char line[EC_SESSION_LINE_MAX];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] == '\0') continue;

        cJSON *obj = cJSON_Parse(line);
        if (!obj) continue;

        if (count >= history_limit) {
            cJSON_Delete(messages[write_idx]);
        }
        messages[write_idx] = obj;
        write_idx = (write_idx + 1) % history_limit;
        if (count < history_limit) count++;
    }
    fclose(f);

    /* Build JSON array preserving full message structure */
    cJSON *arr = cJSON_CreateArray();
    int start = (count < history_limit) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % history_limit;
        cJSON *src = messages[idx];

        cJSON *role = cJSON_GetObjectItem(src, "role");
        if (!cJSON_IsString(role) || !role->valuestring) {
            ESP_LOGW(TAG, "Skipping malformed session entry in %s", path);
            continue;
        }

        cJSON *entry = cJSON_Duplicate(src, 1);
        if (entry) {
            cJSON_DeleteItemFromObject(entry, "ts");
            cJSON_AddItemToArray(arr, entry);
        }
    }

    /* Cleanup ring buffer */
    int cleanup_start = (count < history_limit) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int idx = (cleanup_start + i) % history_limit;
        cJSON_Delete(messages[idx]);
    }

    char *json_str = cJSON_PrintUnformatted(arr);
    ESP_LOGI(TAG, "Session history for %s:\n%s", chat_id, json_str);
    cJSON_Delete(arr);

    if (json_str) {
        strncpy(buf, json_str, size - 1);
        buf[size - 1] = '\0';
        free(json_str);
    } else {
        snprintf(buf, size, "[]");
    }

    return ESP_OK;
}

esp_err_t ec_session_clear(const char *chat_id)
{
    char path[EC_SESSION_PATH_MAX];
    session_path(chat_id, path, sizeof(path));

    if (remove(path) == 0) {
        ESP_LOGI(TAG, "Session %s cleared", chat_id);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

void ec_session_list(void)
{
    DIR *dir = opendir(EC_FS_SESSION_DIR);
    if (!dir) {
        /* SPIFFS is flat, so list all files matching pattern */
        dir = opendir(EC_FS_BASE);
        if (!dir) {
            ESP_LOGW(TAG, "Cannot open SPIFFS directory");
            return;
        }
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "se_") && strstr(entry->d_name, ".jsonl")) {
            ESP_LOGI(TAG, "  Session: %s", entry->d_name);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        ESP_LOGI(TAG, "  No sessions found");
    }
}

/* ==================== [Static Functions] ================================== */

static uint32_t chat_id_hash(const char *s)
{
    uint32_t h = 5381u;
    while (*s) {
        h = ((h << 5) + h) + (unsigned char)*s++;
    }
    return h;
}

static void session_path(const char *chat_id, char *buf, size_t size)
{
    uint32_t h = chat_id_hash(chat_id);
    snprintf(buf, size, "%s/se_%08x.jsonl", EC_FS_SESSION_DIR, (unsigned)h);
}
