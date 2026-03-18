/**
 * @file tools_files.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "core/ec_tools.h"
#include "ec_config_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "cJSON.h"
/* ==================== [Defines] =========================================== */

#define EC_TOOLS_FILES_MAX_FILE_SIZE (32 * 1024)

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static bool validate_path(const char *path);
static esp_err_t replace_first_occurrence(const char *source, const char *old_str,
                                          const char *new_str, char *output, size_t output_size);

static esp_err_t ec_tool_read_file_execute(const char *input_json, char *output, size_t output_size);
static esp_err_t ec_tool_write_file_execute(const char *input_json, char *output, size_t output_size);
static esp_err_t ec_tool_edit_file_execute(const char *input_json, char *output, size_t output_size);
static esp_err_t ec_tool_list_dir_execute(const char *input_json, char *output, size_t output_size);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "tools_files";

static const ec_tools_t s_read_file = {
    .name = "read_file",
    .description = "Read a file from SPIFFS storage. Path must start with " EC_FS_BASE "/.",
    .input_schema_json =
    "{\"type\":\"object\","
    "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " EC_FS_BASE "/\"}},"
    "\"required\":[\"path\"]}",
    .execute = ec_tool_read_file_execute,
};

static const ec_tools_t s_write_file = {
    .name = "write_file",
    .description = "Write or overwrite a file on SPIFFS storage. Path must start with " EC_FS_BASE "/.",
    .input_schema_json =
    "{\"type\":\"object\","
    "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " EC_FS_BASE "/\"},"
    "\"content\":{\"type\":\"string\",\"description\":\"File content to write\"}},"
    "\"required\":[\"path\",\"content\"]}",
    .execute = ec_tool_write_file_execute,
};

static const ec_tools_t s_edit_file = {
    .name = "edit_file",
    .description = "Find and replace text in a file on SPIFFS. Replaces first occurrence of old_string with new_string.",
    .input_schema_json =
    "{\"type\":\"object\","
    "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " EC_FS_BASE "/\"},"
    "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find\"},"
    "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text\"}},"
    "\"required\":[\"path\",\"old_string\",\"new_string\"]}",
    .execute = ec_tool_edit_file_execute,
};

static const ec_tools_t s_list_dir = {
    .name = "list_dir",
    .description = "List files on SPIFFS storage, optionally filtered by path prefix.",
    .input_schema_json =
    "{\"type\":\"object\","
    "\"properties\":{\"prefix\":{\"type\":\"string\",\"description\":\"Optional path prefix filter, e.g. " EC_FS_BASE "/memory/\"}},"
    "\"required\":[]}",
    .execute = ec_tool_list_dir_execute,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_tools_read_file(void)
{
    ec_tools_register(&s_read_file);
    return ESP_OK;
}

esp_err_t ec_tools_write_file(void)
{
    ec_tools_register(&s_write_file);
    return ESP_OK;
}

esp_err_t ec_tools_edit_file(void)
{
    ec_tools_register(&s_edit_file);
    return ESP_OK;
}

esp_err_t ec_tools_list_dir(void)
{
    ec_tools_register(&s_list_dir);
    return ESP_OK;
}
/* ==================== [Static Functions] ================================== */

static esp_err_t ec_tool_read_file_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    if (!validate_path(path)) {
        snprintf(output, output_size, "Error: path must start with %s/ and must not contain '..'", EC_FS_BASE);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(output, output_size, "Error: file not found: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    size_t max_read = output_size - 1;
    if (max_read > EC_TOOLS_FILES_MAX_FILE_SIZE) {
        max_read = EC_TOOLS_FILES_MAX_FILE_SIZE;
    }

    size_t n = fread(output, 1, max_read, f);
    output[n] = '\0';
    fclose(f);

    ESP_LOGI(TAG, "read_file: %s (%d bytes)", path, (int)n);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ec_tool_write_file_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));

    if (!validate_path(path)) {
        snprintf(output, output_size, "Error: path must start with %s/ and must not contain '..'", EC_FS_BASE);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (!content) {
        snprintf(output, output_size, "Error: missing 'content' field");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(output, output_size, "Error: cannot open file for writing: %s", path);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);

    if (written != len) {
        snprintf(output, output_size, "Error: wrote %d of %d bytes to %s", (int)written, (int)len, path);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    snprintf(output, output_size, "OK: wrote %d bytes to %s", (int)written, path);
    ESP_LOGI(TAG, "write_file: %s (%d bytes)", path, (int)written);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ec_tool_edit_file_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = cJSON_GetStringValue(cJSON_GetObjectItem(root, "path"));
    const char *old_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "old_string"));
    const char *new_str = cJSON_GetStringValue(cJSON_GetObjectItem(root, "new_string"));

    if (!validate_path(path)) {
        snprintf(output, output_size, "Error: path must start with %s/ and must not contain '..'", EC_FS_BASE);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (!old_str || !new_str) {
        snprintf(output, output_size, "Error: missing 'old_string' or 'new_string' field");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    /* Read existing file */
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(output, output_size, "Error: file not found: %s", path);
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > EC_TOOLS_FILES_MAX_FILE_SIZE) {
        snprintf(output, output_size, "Error: file too large or empty (%ld bytes)", file_size);
        fclose(f);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_SIZE;
    }

    char *buf = malloc(file_size + 1);
    char *result = malloc((size_t)file_size + strlen(new_str) + 1);
    if (!buf || !result) {
        free(buf);
        free(result);
        fclose(f);
        snprintf(output, output_size, "Error: out of memory");
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }

    size_t n = fread(buf, 1, file_size, f);
    buf[n] = '\0';
    fclose(f);

    esp_err_t replace_err = replace_first_occurrence(buf, old_str, new_str, result, (size_t)file_size + strlen(new_str) + 1);
    free(buf);
    if (replace_err != ESP_OK) {
        snprintf(output, output_size, "Error: old_string not found in %s", path);
        free(result);
        cJSON_Delete(root);
        return replace_err;
    }

    /* Write back */
    f = fopen(path, "w");
    if (!f) {
        snprintf(output, output_size, "Error: cannot open file for writing: %s", path);
        free(result);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    size_t total = strlen(result);
    fwrite(result, 1, total, f);
    fclose(f);
    free(result);

    snprintf(output, output_size, "OK: edited %s (replaced %d bytes with %d bytes)",
             path, (int)strlen(old_str), (int)strlen(new_str));
    ESP_LOGI(TAG, "edit_file: %s", path);
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t ec_tool_list_dir_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    const char *prefix = NULL;
    if (root) {
        cJSON *pfx = cJSON_GetObjectItem(root, "prefix");
        if (pfx && cJSON_IsString(pfx)) {
            prefix = pfx->valuestring;
        }
    }

    DIR *dir = opendir(EC_FS_BASE);
    if (!dir) {
        snprintf(output, output_size, "Error: cannot open %s directory", EC_FS_BASE);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    size_t off = 0;
    struct dirent *ent;
    int count = 0;

    while ((ent = readdir(dir)) != NULL && off < output_size - 1) {
        /* Build full path: SPIFFS entries are just filenames with embedded slashes */
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", EC_FS_BASE, ent->d_name);

        if (prefix && strncmp(full_path, prefix, strlen(prefix)) != 0) {
            continue;
        }

        off += snprintf(output + off, output_size - off, "%s\n", full_path);
        count++;
    }

    closedir(dir);

    if (count == 0) {
        snprintf(output, output_size, "(no files found)");
    }

    ESP_LOGI(TAG, "list_dir: %d files (prefix=%s)", count, prefix ? prefix : "(none)");
    cJSON_Delete(root);
    return ESP_OK;
}

static bool validate_path(const char *path)
{
    if (!path) {
        return false;
    }
    size_t base_len = strlen(EC_FS_BASE);
    if (strncmp(path, EC_FS_BASE, base_len) != 0) {
        return false;
    }
    /* Require a path separator after the base (unless base ends with '/') */
    if (base_len > 0 && EC_FS_BASE[base_len - 1] != '/') {
        if (path[base_len] != '/') {
            return false;
        }
    }
    if (strstr(path, "..") != NULL) {
        return false;
    }
    return true;
}

static esp_err_t replace_first_occurrence(const char *source, const char *old_str,
                                          const char *new_str, char *output, size_t output_size)
{
    const char *pos;
    size_t prefix_len;
    size_t old_len;
    size_t new_len;
    size_t suffix_len;
    size_t total_len;

    if (!source || !old_str || !new_str || !output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    pos = strstr(source, old_str);
    if (!pos) {
        return ESP_ERR_NOT_FOUND;
    }

    prefix_len = (size_t)(pos - source);
    old_len = strlen(old_str);
    new_len = strlen(new_str);
    suffix_len = strlen(pos + old_len);
    total_len = prefix_len + new_len + suffix_len;

    if (total_len + 1 > output_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(output, source, prefix_len);
    memcpy(output + prefix_len, new_str, new_len);
    memcpy(output + prefix_len + new_len, pos + old_len, suffix_len);
    output[total_len] = '\0';

    return ESP_OK;
}
