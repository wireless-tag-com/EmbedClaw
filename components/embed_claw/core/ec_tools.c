/**
 * @file ec_tools.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "ec_tools.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

#define EC_TOOLS_REG_EXTERN
#include "tools/ec_tools_reg.inc"

static void build_tools_json(void);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "tools";

static const ec_tools_t *s_tools[_EC_TOOLS_ENMU_MAX] = {0};
static char *s_tools_json = NULL;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_tools_register_all(void)
{
#define EC_TOOLS_REG_FUNC
#include "tools/ec_tools_reg.inc"

    build_tools_json();

    return ESP_OK;
}

esp_err_t ec_tools_register(const ec_tools_t *tool)
{
    if (!tool || !tool->name || tool->name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < _EC_TOOLS_ENMU_MAX; i++) {
        if (!s_tools[i]) {
            continue;
        }
        if (s_tools[i] == tool || strcmp(s_tools[i]->name, tool->name) == 0) {
            return ESP_OK;
        }
    }

    for (size_t i = 0; i < _EC_TOOLS_ENMU_MAX; i++) {
        if (!s_tools[i]) {
            s_tools[i] = tool;
            return ESP_OK;
        }

    }

    ESP_LOGE(TAG, "Tool registry full, cannot register: %s", tool->name);
    return ESP_ERR_NO_MEM;
}

esp_err_t ec_tools_execute(const char *name, const char *input_json,
                           char *output, size_t output_size)
{
    for (size_t i = 0; i < _EC_TOOLS_ENMU_MAX; i++) {
        if (!s_tools[i]) {
            continue;
        }
        if (s_tools[i]->name == name) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i]->execute(input_json, output, output_size);
        }
    }

    for (size_t i = 0; i < _EC_TOOLS_ENMU_MAX; i++) {
        if (!s_tools[i]) {
            continue;
        }
        if (strcmp(s_tools[i]->name, name) == 0) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i]->execute(input_json, output, output_size);
        }
    }

    ESP_LOGW(TAG, "Unknown tool: %s", name);
    snprintf(output, output_size, "Error: unknown tool '%s'", name);
    return ESP_ERR_NOT_FOUND;
}

const char *ec_tools_get_json(void)
{
    return s_tools_json;
}

size_t ec_tools_build_summary(char *buf, size_t size)
{
    size_t off = 0;

    if (!buf || size == 0) {
        return 0;
    }

    for (size_t i = 0; i < _EC_TOOLS_ENMU_MAX && off < size - 1; i++) {
        const char *name = NULL;
        const char *description = NULL;

        if (!s_tools[i]) {
            continue;
        }

        name = s_tools[i]->name ? s_tools[i]->name : "(unnamed)";
        description = s_tools[i]->description ? s_tools[i]->description : "";

        off += snprintf(buf + off, size - off, "- %s: %s\n", name, description);
        if (off >= size) {
            off = size - 1;
        }
    }

    buf[off] = '\0';
    return off;
}

void ec_tools_free_json(void)
{
    cJSON_free(s_tools_json);
    s_tools_json = NULL;
}

/* ==================== [Static Functions] ================================== */

static void build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < _EC_TOOLS_ENMU_MAX; i++) {
        if (!s_tools[i]) {
            continue;
        }

        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i]->name);
        cJSON_AddStringToObject(tool, "description", s_tools[i]->description);

        cJSON *schema = cJSON_Parse(s_tools[i]->input_schema_json);
        if (schema) {
            cJSON_AddItemToObject(tool, "input_schema", schema);
        }

        cJSON_AddItemToArray(arr, tool);
    }

    cJSON_free(s_tools_json);
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

}
