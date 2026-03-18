/**
 * @file tools_web_search.c
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

#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "cJSON.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} search_buf_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void format_results_tavily(cJSON *root, char *output, size_t output_size);
static esp_err_t tavily_search(const char *query_str, char *output, size_t output_size);

static esp_err_t ec_tool_web_search_execute(const char *input_json, char *output, size_t output_size);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "web_search";

static const ec_tools_t s_web_search = {
    .name = "web_search",
    .description = "Search the web for current information. Use this when you need up-to-date facts, news, weather, or anything beyond your training data.",
    .input_schema_json =
    "{\"type\":\"object\","
    "\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"The search query\"}},"
    "\"required\":[\"query\"]}",
    .execute = ec_tool_web_search_execute,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_tools_web_search(void)
{
    ec_tools_register(&s_web_search);
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    search_buf_t *sb = (search_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        size_t needed = sb->len + evt->data_len;
        if (needed < sb->cap) {
            memcpy(sb->data + sb->len, evt->data, evt->data_len);
            sb->len += evt->data_len;
            sb->data[sb->len] = '\0';
        }
    }
    return ESP_OK;
}

static void format_results_tavily(cJSON *root, char *output, size_t output_size)
{
    cJSON *results = cJSON_GetObjectItem(root, "results");
    if (!results || !cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        snprintf(output, output_size, "No web results found.");
        return;
    }

    size_t off = 0;
    int idx = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, results) {
        if (idx >= EC_SEARCH_RESULT_COUNT) {
            break;
        }

        cJSON *title = cJSON_GetObjectItem(item, "title");
        cJSON *url = cJSON_GetObjectItem(item, "url");
        cJSON *content = cJSON_GetObjectItem(item, "content");

        off += snprintf(output + off, output_size - off,
                        "%d. %s\n   %s\n   %s\n\n",
                        idx + 1,
                        (title && cJSON_IsString(title)) ? title->valuestring : "(no title)",
                        (url && cJSON_IsString(url)) ? url->valuestring : "",
                        (content && cJSON_IsString(content)) ? content->valuestring : "");

        if (off >= output_size - 1) {
            break;
        }
        idx++;
    }
}

static esp_err_t tavily_search(const char *query_str, char *output, size_t output_size)
{
    ESP_LOGI(TAG, "Query: %s", query_str);
    cJSON *body = cJSON_CreateObject();
    if (!body) {
        snprintf(output, output_size, "Error: Out of memory (JSON)");
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(body, "query", query_str);
    cJSON_AddNumberToObject(body, "max_results", EC_SEARCH_RESULT_COUNT);
    cJSON_AddStringToObject(body, "search_depth", "basic");
    cJSON_AddStringToObject(body, "topic", "general");

    char *post_data = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!post_data) {
        snprintf(output, output_size, "Error: Failed to build request JSON");
        return ESP_ERR_NO_MEM;
    }

    search_buf_t sb = {0};
    sb.data = calloc(1, EC_SEARCH_BUF_SIZE);

    if (!sb.data) {
        free(post_data);
        snprintf(output, output_size, "Error: Out of memory");
        ESP_LOGE(TAG, "Error: Out of memory");
        return ESP_ERR_NO_MEM;
    }

    sb.cap = EC_SEARCH_BUF_SIZE;

    esp_http_client_config_t config = {
        .url = "https://api.tavily.com/search",
        .event_handler = http_event_handler,
        .user_data = &sb,
        .timeout_ms = 15000,
        .buffer_size = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .max_redirection_count = 3,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(post_data);
        free(sb.data);
        snprintf(output, output_size, "Error: HTTP client init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");
    esp_http_client_set_header(client, "User-Agent", "curl/8.4.0");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    /* Heap-allocated so header value stays valid for entire perform() (some builds may not copy) */
    size_t auth_len = 8 + strlen(EC_SECRET_SEARCH_KEY);
    char *auth_hdr = (char *)malloc(auth_len);
    if (!auth_hdr) {
        free(post_data);
        free(sb.data);
        snprintf(output, output_size, "Error: Out of memory");
        return ESP_ERR_NO_MEM;
    }
    snprintf(auth_hdr, auth_len, "Bearer %s", EC_SECRET_SEARCH_KEY);
    esp_http_client_set_header(client, "Authorization", auth_hdr);

    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    free(auth_hdr);
    free(post_data);

    if (err != ESP_OK) {
        free(sb.data);
        snprintf(output, output_size, "Error: HTTP request failed (%s)", esp_err_to_name(err));
        return err;
    }
    if (status != 200) {
        ESP_LOGE(TAG, "Tavily HTTP %d, body: %.250s", status, sb.data ? sb.data : "(null)");
        cJSON *err_root = cJSON_Parse(sb.data);
        free(sb.data);
        if (err_root) {
            cJSON *detail = cJSON_GetObjectItem(err_root, "detail");
            if (detail && cJSON_IsObject(detail)) {
                cJSON *err_obj = cJSON_GetObjectItem(detail, "error");
                if (err_obj && cJSON_IsString(err_obj)) {
                    snprintf(output, output_size, "Error: Tavily API %d: %s", status, err_obj->valuestring);
                    ESP_LOGE(TAG, "Tavily %d: %s", status, err_obj->valuestring);
                    cJSON_Delete(err_root);
                    return ESP_FAIL;
                }
            }
            cJSON_Delete(err_root);
        }
        snprintf(output, output_size, "Error: Tavily API returned %d", status);
        ESP_LOGE(TAG, "Tavily HTTP %d (no detail in body)", status);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(sb.data);
    free(sb.data);

    if (!root) {
        snprintf(output, output_size, "Error: Failed to parse Tavily search results");
        return ESP_FAIL;
    }

    /* Tavily error in body */
    cJSON *detail = cJSON_GetObjectItem(root, "detail");
    if (detail && cJSON_IsObject(detail)) {
        cJSON *err_obj = cJSON_GetObjectItem(detail, "error");
        if (err_obj && cJSON_IsString(err_obj)) {
            snprintf(output, output_size, "Error: Tavily API: %s", err_obj->valuestring);
            cJSON_Delete(root);
            return ESP_FAIL;
        }
    }

    format_results_tavily(root, output, output_size);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Tavily search complete, %d bytes result", (int)strlen(output));
    return ESP_OK;
}

static esp_err_t ec_tool_web_search_execute(const char *input_json, char *output, size_t output_size)
{
    if (EC_SECRET_SEARCH_KEY[0] == '\0') {
        snprintf(output, output_size, "Error: No Tavily API key. Set EC_SECRET_SEARCH_KEY or use CLI: set_search_key <KEY>");
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: Invalid input JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *query = cJSON_GetObjectItem(input, "query");
    if (!query || !cJSON_IsString(query) || query->valuestring[0] == '\0') {
        cJSON_Delete(input);
        snprintf(output, output_size, "Error: Missing 'query' field");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Searching: %s", query->valuestring);

    esp_err_t err = tavily_search(query->valuestring, output, output_size);
    cJSON_Delete(input);
    return err;
}
