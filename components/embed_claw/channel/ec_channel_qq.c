/**
 * @file ec_channel_qq.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-13
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "ec_config_internal.h"

#if EC_QQ_ENABLE

#include <stdbool.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "core/ec_agent.h"
#include "core/ec_channel.h"

/* ==================== [Defines] =========================================== */

#define EC_QQ_TOKEN_URL            "https://bots.qq.com/app/getAppAccessToken"
#define EC_QQ_API_BASE             "https://api.sgroup.qq.com"
#define EC_QQ_TASK_STACK           (8 * 1024)
#define EC_QQ_TASK_PRIO            5
#define EC_QQ_HTTP_TIMEOUT_MS      10000
#define EC_QQ_GATEWAY_URL_MAX      256
#define EC_QQ_ACCESS_TOKEN_MAX     512
#define EC_QQ_SESSION_ID_MAX       128
#define EC_QQ_HTTP_RESP_INIT_CAP   1024
#define EC_QQ_HTTP_LOG_BUF_SIZE    192
#define EC_QQ_WS_LOOP_DELAY_MS     200

/* ==================== [Typedefs] ========================================== */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} http_resp_t;

/* ==================== [Static Prototypes] ================================= */

static void http_resp_append(http_resp_t *r, const void *data, size_t len);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static void http_body_snippet_redacted(const http_resp_t *resp, char *buf, size_t buf_size);
static bool json_int_value(cJSON *item, int *out);
static bool fetch_access_token(void);
static bool ensure_access_token(void);
static bool fetch_gateway_url(void);
static bool qq_send_ws_json(cJSON *payload);
static bool qq_send_identify_frame(void);
static bool qq_send_resume_frame(void);
static bool qq_send_heartbeat(void);
static esp_err_t qq_decode_dispatch_event(const char *event_type, cJSON *data, ec_msg_t *msg);
static void qq_handle_dispatch_event(const char *event_type, cJSON *data);
static bool qq_build_api_path(const ec_msg_t *msg, char *path, size_t path_size);
static char *qq_build_message_body_alloc(const ec_msg_t *msg);
static bool qq_post_message(const ec_msg_t *msg);
static void handle_ws_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data);
static void qq_ws_task(void *arg);

static esp_err_t ec_channel_qq_start(void);
static esp_err_t ec_channel_qq_send(const ec_msg_t *msg);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "qq";

static char s_access_token[EC_QQ_ACCESS_TOKEN_MAX] = "";
static int64_t s_token_expire_at_us = 0;
static char s_gateway_url[EC_QQ_GATEWAY_URL_MAX] = "";
static char s_session_id[EC_QQ_SESSION_ID_MAX] = "";
static int s_last_seq = -1;
static int s_heartbeat_interval_ms = 0;
static int64_t s_last_heartbeat_us = 0;
static bool s_connected = false;
static bool s_need_reconnect = false;
static TaskHandle_t s_ws_task = NULL;
static esp_websocket_client_handle_t s_ws_client = NULL;

static const ec_channel_t s_driver = {
    .name = g_ec_channel_qq,
    .vtable = {
        .start = ec_channel_qq_start,
        .send = ec_channel_qq_send,
    }
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_channel_qq(void)
{
    return ec_channel_register(&s_driver);
}

/* ==================== [Static Functions] ================================== */

static void http_resp_append(http_resp_t *r, const void *data, size_t len)
{
    if (!r || !data || len == 0) {
        return;
    }

    if (r->len + len + 1 > r->cap) {
        size_t new_cap = r->cap ? r->cap * 2 : EC_QQ_HTTP_RESP_INIT_CAP;
        if (new_cap < r->len + len + 1) {
            new_cap = r->len + len + 1;
        }

        char *tmp = realloc(r->buf, new_cap);
        if (!tmp) {
            return;
        }

        r->buf = tmp;
        r->cap = new_cap;
    }

    memcpy(r->buf + r->len, data, len);
    r->len += len;
    r->buf[r->len] = '\0';
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_t *r = (http_resp_t *)evt->user_data;

    if (evt->event_id == HTTP_EVENT_ON_DATA && r) {
        http_resp_append(r, evt->data, evt->data_len);
    }

    return ESP_OK;
}

static void http_body_snippet_redacted(const http_resp_t *resp, char *buf, size_t buf_size)
{
    char *token_start;
    char *token_end;

    if (!buf || buf_size == 0) {
        return;
    }

    if (!resp || !resp->buf || resp->buf[0] == '\0') {
        snprintf(buf, buf_size, "(empty)");
        return;
    }

    snprintf(buf, buf_size, "%s", resp->buf);

    token_start = strstr(buf, "\"access_token\":\"");
    if (!token_start) {
        return;
    }

    token_start += strlen("\"access_token\":\"");
    token_end = strchr(token_start, '"');
    if (!token_end) {
        return;
    }

    memmove(token_start + 3, token_end, strlen(token_end) + 1);
    memcpy(token_start, "***", 3);
}

static bool json_int_value(cJSON *item, int *out)
{
    char *end = NULL;
    long parsed;

    if (!item || !out) {
        return false;
    }

    if (cJSON_IsNumber(item)) {
        *out = item->valueint;
        return true;
    }

    if (!cJSON_IsString(item) || !item->valuestring || item->valuestring[0] == '\0') {
        return false;
    }

    parsed = strtol(item->valuestring, &end, 10);
    if (!end || *end != '\0' || parsed <= 0 || parsed > INT_MAX) {
        return false;
    }

    *out = (int)parsed;
    return true;
}

static bool fetch_access_token(void)
{
    cJSON *body = cJSON_CreateObject();
    char *json_str;
    http_resp_t resp = {0};
    esp_http_client_handle_t client;
    esp_http_client_config_t cfg = {
        .url = EC_QQ_TOKEN_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = EC_QQ_HTTP_TIMEOUT_MS,
        .buffer_size = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_err_t err;
    int status = 0;
    int expires_in_s = 0;
    bool ok = false;
    char body_log[EC_QQ_HTTP_LOG_BUF_SIZE];

    if (!body) {
        return false;
    }

    cJSON_AddStringToObject(body, "appId", EC_QQ_APP_ID);
    cJSON_AddStringToObject(body, "clientSecret", EC_QQ_CLIENT_SECRET);
    json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!json_str) {
        return false;
    }

    resp.buf = calloc(1, EC_QQ_HTTP_RESP_INIT_CAP);
    if (!resp.buf) {
        free(json_str);
        return false;
    }
    resp.cap = EC_QQ_HTTP_RESP_INIT_CAP;

    client = esp_http_client_init(&cfg);
    if (!client) {
        free(resp.buf);
        free(json_str);
        return false;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));
    ESP_LOGI(TAG, "Requesting QQ access token");
    err = esp_http_client_perform(client);
    free(json_str);
    status = esp_http_client_get_status_code(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "QQ token request failed: err=%s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "QQ token response: http=%d", status);
    }

    if (err == ESP_OK && resp.buf) {
        cJSON *root = cJSON_Parse(resp.buf);
        if (root) {
            const char *token = cJSON_GetStringValue(cJSON_GetObjectItem(root, "access_token"));
            cJSON *expires_in = cJSON_GetObjectItem(root, "expires_in");
            if (token && json_int_value(expires_in, &expires_in_s)) {
                snprintf(s_access_token, sizeof(s_access_token), "%s", token);
                s_token_expire_at_us = esp_timer_get_time() + (int64_t)expires_in_s * 1000000LL;
                ok = true;
                ESP_LOGI(TAG, "QQ access token refreshed, expires_in=%d", expires_in_s);
            } else {
                http_body_snippet_redacted(&resp, body_log, sizeof(body_log));
                ESP_LOGW(TAG, "QQ token response missing access_token/expires_in: %.160s",
                         body_log);
            }
            cJSON_Delete(root);
        } else {
            http_body_snippet_redacted(&resp, body_log, sizeof(body_log));
            ESP_LOGW(TAG, "QQ token response is not valid JSON: %.160s", body_log);
        }
    }

    if (!ok && err == ESP_OK) {
        http_body_snippet_redacted(&resp, body_log, sizeof(body_log));
        ESP_LOGW(TAG, "QQ token request rejected: http=%d body=%.160s", status, body_log);
    }

    esp_http_client_cleanup(client);
    free(resp.buf);
    return ok;
}

static bool ensure_access_token(void)
{
    int64_t now = esp_timer_get_time();

    if (s_access_token[0] != '\0' && now < s_token_expire_at_us - 300000000LL) {
        ESP_LOGD(TAG, "QQ access token still valid");
        return true;
    }

    ESP_LOGI(TAG, "QQ access token missing or expiring soon, refreshing");
    return fetch_access_token();
}

static bool fetch_gateway_url(void)
{
    char url[sizeof(EC_QQ_API_BASE) + 16];
    char auth[EC_QQ_ACCESS_TOKEN_MAX + 16];
    http_resp_t resp = {0};
    esp_http_client_handle_t client;
    esp_http_client_config_t cfg;
    esp_err_t err;
    int status = 0;
    bool ok = false;
    char body_log[EC_QQ_HTTP_LOG_BUF_SIZE];

    if (!ensure_access_token()) {
        return false;
    }

    snprintf(url, sizeof(url), "%s/gateway", EC_QQ_API_BASE);
    snprintf(auth, sizeof(auth), "QQBot %s", s_access_token);

    resp.buf = calloc(1, EC_QQ_HTTP_RESP_INIT_CAP);
    if (!resp.buf) {
        return false;
    }
    resp.cap = EC_QQ_HTTP_RESP_INIT_CAP;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = url;
    cfg.method = HTTP_METHOD_GET;
    cfg.event_handler = http_event_handler;
    cfg.user_data = &resp;
    cfg.timeout_ms = EC_QQ_HTTP_TIMEOUT_MS;
    cfg.buffer_size = 1024;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    client = esp_http_client_init(&cfg);
    if (!client) {
        free(resp.buf);
        return false;
    }

    esp_http_client_set_header(client, "Authorization", auth);
    ESP_LOGI(TAG, "Requesting QQ gateway URL");
    err = esp_http_client_perform(client);
    status = esp_http_client_get_status_code(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "QQ gateway request failed: err=%s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "QQ gateway response: http=%d", status);
    }

    if (err == ESP_OK && resp.buf) {
        cJSON *root = cJSON_Parse(resp.buf);
        if (root) {
            const char *gateway = cJSON_GetStringValue(cJSON_GetObjectItem(root, "url"));
            if (gateway && gateway[0] != '\0') {
                snprintf(s_gateway_url, sizeof(s_gateway_url), "%s", gateway);
                ok = true;
                ESP_LOGI(TAG, "QQ gateway URL acquired: %.120s", s_gateway_url);
            } else {
                http_body_snippet_redacted(&resp, body_log, sizeof(body_log));
                ESP_LOGW(TAG, "QQ gateway response missing url: %.160s", body_log);
            }
            cJSON_Delete(root);
        } else {
            http_body_snippet_redacted(&resp, body_log, sizeof(body_log));
            ESP_LOGW(TAG, "QQ gateway response is not valid JSON: %.160s", body_log);
        }
    }

    if (!ok && err == ESP_OK) {
        http_body_snippet_redacted(&resp, body_log, sizeof(body_log));
        ESP_LOGW(TAG, "QQ gateway request rejected: http=%d body=%.160s", status, body_log);
    }

    esp_http_client_cleanup(client);
    free(resp.buf);
    return ok;
}

static bool qq_send_ws_json(cJSON *payload)
{
    char *json_str;
    int sent;

    if (!payload || !s_ws_client || !esp_websocket_client_is_connected(s_ws_client)) {
        return false;
    }

    json_str = cJSON_PrintUnformatted(payload);
    cJSON_Delete(payload);
    if (!json_str) {
        return false;
    }

    sent = esp_websocket_client_send_text(s_ws_client, json_str, strlen(json_str), portMAX_DELAY);
    if (sent < 0) {
        ESP_LOGW(TAG, "QQ websocket send failed: %.160s", json_str);
    }
    free(json_str);
    return sent >= 0;
}

static bool qq_send_identify_frame(void)
{
    cJSON *payload = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char token[EC_QQ_ACCESS_TOKEN_MAX + 16];
    cJSON *shard;

    if (!payload || !data) {
        cJSON_Delete(payload);
        cJSON_Delete(data);
        return false;
    }

    snprintf(token, sizeof(token), "QQBot %s", s_access_token);

    cJSON_AddNumberToObject(payload, "op", 2);
    cJSON_AddItemToObject(payload, "d", data);
    cJSON_AddStringToObject(data, "token", token);
    cJSON_AddNumberToObject(data, "intents", EC_QQ_INTENTS);
    shard = cJSON_AddArrayToObject(data, "shard");
    if (shard) {
        cJSON_AddItemToArray(shard, cJSON_CreateNumber(0));
        cJSON_AddItemToArray(shard, cJSON_CreateNumber(1));
    }

    ESP_LOGI(TAG, "Sending QQ IDENTIFY, intents=0x%x", EC_QQ_INTENTS);
    return qq_send_ws_json(payload);
}

static bool qq_send_resume_frame(void)
{
    cJSON *payload = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    char token[EC_QQ_ACCESS_TOKEN_MAX + 16];

    if (!payload || !data) {
        cJSON_Delete(payload);
        cJSON_Delete(data);
        return false;
    }

    snprintf(token, sizeof(token), "QQBot %s", s_access_token);

    cJSON_AddNumberToObject(payload, "op", 6);
    cJSON_AddItemToObject(payload, "d", data);
    cJSON_AddStringToObject(data, "token", token);
    cJSON_AddStringToObject(data, "session_id", s_session_id);
    cJSON_AddNumberToObject(data, "seq", s_last_seq);

    ESP_LOGI(TAG, "Sending QQ RESUME, seq=%d", s_last_seq);
    return qq_send_ws_json(payload);
}

static bool qq_send_heartbeat(void)
{
    cJSON *payload = cJSON_CreateObject();

    if (!payload) {
        return false;
    }

    cJSON_AddNumberToObject(payload, "op", 1);
    if (s_last_seq >= 0) {
        cJSON_AddNumberToObject(payload, "d", s_last_seq);
    } else {
        cJSON_AddNullToObject(payload, "d");
    }

    s_last_heartbeat_us = esp_timer_get_time();
    ESP_LOGD(TAG, "Sending QQ heartbeat, seq=%d", s_last_seq);
    return qq_send_ws_json(payload);
}

static void qq_handle_dispatch_event(const char *event_type, cJSON *data)
{
    ec_msg_t msg = {0};

    if (qq_decode_dispatch_event(event_type, data, &msg) != ESP_OK) {
        return;
    }

    snprintf(msg.channel, sizeof(msg.channel), "%s", g_ec_channel_qq);

    esp_err_t err = ec_agent_inbound(&msg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Inbound queue full, drop qq message");
        free(msg.content);
    }
}

static esp_err_t qq_decode_dispatch_event(const char *event_type, cJSON *data, ec_msg_t *msg)
{
    const char *content;
    const char *id;
    char chat_id[64];
    char chat_type[16];

    if (!event_type || !data || !msg) {
        return ESP_ERR_INVALID_ARG;
    }

    content = cJSON_GetStringValue(cJSON_GetObjectItem(data, "content"));
    if (!content || content[0] == '\0') {
        return ESP_ERR_NOT_FOUND;
    }

    if (strcmp(event_type, "C2C_MESSAGE_CREATE") == 0) {
        cJSON *author = cJSON_GetObjectItem(data, "author");
        id = author ? cJSON_GetStringValue(cJSON_GetObjectItem(author, "user_openid")) : NULL;
        if (!id || id[0] == '\0') {
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(chat_type, sizeof(chat_type), "c2c");
        snprintf(chat_id, sizeof(chat_id), "%s", id);
    } else if (strcmp(event_type, "GROUP_AT_MESSAGE_CREATE") == 0) {
        id = cJSON_GetStringValue(cJSON_GetObjectItem(data, "group_openid"));
        if (!id || id[0] == '\0') {
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(chat_type, sizeof(chat_type), "group");
        snprintf(chat_id, sizeof(chat_id), "%s", id);
    } else if (strcmp(event_type, "AT_MESSAGE_CREATE") == 0) {
        id = cJSON_GetStringValue(cJSON_GetObjectItem(data, "channel_id"));
        if (!id || id[0] == '\0') {
            return ESP_ERR_INVALID_ARG;
        }
        snprintf(chat_type, sizeof(chat_type), "channel");
        snprintf(chat_id, sizeof(chat_id), "%s", id);
    } else {
        return ESP_ERR_NOT_FOUND;
    }

    memset(msg, 0, sizeof(*msg));
    snprintf(msg->channel, sizeof(msg->channel), "%s", g_ec_channel_qq);
    snprintf(msg->chat_id, sizeof(msg->chat_id), "%s", chat_id);
    snprintf(msg->chat_type, sizeof(msg->chat_type), "%s", chat_type);
    msg->content = strdup(content);
    if (!msg->content) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}


static bool qq_build_api_path(const ec_msg_t *msg, char *path, size_t path_size)
{
    if (!msg || !path || path_size == 0 ||
            msg->chat_id[0] == '\0' || msg->chat_type[0] == '\0') {
        return false;
    }

    if (strcmp(msg->chat_type, "c2c") == 0) {
        snprintf(path, path_size, "/v2/users/%s/messages", msg->chat_id);
    } else if (strcmp(msg->chat_type, "group") == 0) {
        snprintf(path, path_size, "/v2/groups/%s/messages", msg->chat_id);
    } else if (strcmp(msg->chat_type, "channel") == 0) {
        snprintf(path, path_size, "/channels/%s/messages", msg->chat_id);
    } else {
        return false;
    }

    return true;
}

static char *qq_build_message_body_alloc(const ec_msg_t *msg)
{
    cJSON *body;
    char *body_str;

    if (!msg || !msg->content || msg->content[0] == '\0' || msg->chat_type[0] == '\0') {
        return NULL;
    }

    body = cJSON_CreateObject();
    if (!body) {
        return NULL;
    }

    cJSON_AddStringToObject(body, "content", msg->content);
    if (strcmp(msg->chat_type, "channel") != 0) {
        cJSON_AddNumberToObject(body, "msg_type", 0);
        cJSON_AddNumberToObject(body, "msg_seq", 1);
    }


    body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    return body_str;
}

static bool qq_post_message(const ec_msg_t *msg)
{
    char path[96];
    char url[EC_QQ_GATEWAY_URL_MAX];
    char auth[EC_QQ_ACCESS_TOKEN_MAX + 16];
    http_resp_t resp = {0};
    esp_http_client_handle_t client;
    esp_http_client_config_t cfg;
    esp_err_t err;
    char *body_str = NULL;
    bool ok = false;

    if (!msg || !msg->content || msg->content[0] == '\0' ||
            msg->chat_id[0] == '\0' || msg->chat_type[0] == '\0') {
        return false;
    }

    if (!qq_build_api_path(msg, path, sizeof(path))) {
        return false;
    }

    if (!ensure_access_token()) {
        return false;
    }

    snprintf(url, sizeof(url), "%s%s", EC_QQ_API_BASE, path);
    body_str = qq_build_message_body_alloc(msg);
    if (!body_str) {
        return false;
    }

    resp.buf = calloc(1, EC_QQ_HTTP_RESP_INIT_CAP);
    if (!resp.buf) {
        free(body_str);
        return false;
    }
    resp.cap = EC_QQ_HTTP_RESP_INIT_CAP;

    memset(&cfg, 0, sizeof(cfg));
    cfg.url = url;
    cfg.method = HTTP_METHOD_POST;
    cfg.event_handler = http_event_handler;
    cfg.user_data = &resp;
    cfg.timeout_ms = EC_QQ_HTTP_TIMEOUT_MS;
    cfg.buffer_size = 1024;
    cfg.crt_bundle_attach = esp_crt_bundle_attach;

    client = esp_http_client_init(&cfg);
    if (!client) {
        free(body_str);
        free(resp.buf);
        return false;
    }

    snprintf(auth, sizeof(auth), "QQBot %s", s_access_token);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body_str, strlen(body_str));
    err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ok = status >= 200 && status < 300;
        if (!ok) {
            ESP_LOGW(TAG, "QQ send failed, status=%d body=%.120s", status, resp.buf ? resp.buf : "");
        }
    }

    esp_http_client_cleanup(client);
    free(body_str);
    free(resp.buf);
    return ok;
}

static void handle_ws_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;

    (void)arg;
    (void)base;

    switch (event_id) {
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_CLOSED:
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGW(TAG, "QQ websocket event=%" PRId32 ", reconnect scheduled", event_id);
        s_connected = false;
        s_need_reconnect = true;
        break;

    case WEBSOCKET_EVENT_DATA: {
        cJSON *root;
        cJSON *op_item;
        cJSON *d_item;
        cJSON *s_item;
        const char *event_type;
        cJSON *t_item;

        if (!data || data->op_code != WS_TRANSPORT_OPCODES_TEXT) {
            break;
        }

        if (data->payload_offset != 0 || !data->fin || data->payload_len != data->data_len) {
            ESP_LOGW(TAG, "Ignoring fragmented QQ websocket frame");
            break;
        }

        ESP_LOGI(TAG, "QQ WS: full payload (%u bytes):", (unsigned)data->data_len);
        ESP_LOGI(TAG, "%.*s", (int)data->data_len, data->data_ptr);

        root = cJSON_ParseWithLength(data->data_ptr, (size_t)data->data_len);
        if (!root) {
            break;
        }

        op_item = cJSON_GetObjectItem(root, "op");
        d_item = cJSON_GetObjectItem(root, "d");
        s_item = cJSON_GetObjectItem(root, "s");
        t_item = cJSON_GetObjectItem(root, "t");
        event_type = cJSON_IsString(t_item) ? t_item->valuestring : NULL;

        if (cJSON_IsNumber(s_item)) {
            s_last_seq = s_item->valueint;
        }

        if (!cJSON_IsNumber(op_item)) {
            cJSON_Delete(root);
            break;
        }

        switch (op_item->valueint) {
        case 10: {
            cJSON *heartbeat = d_item ? cJSON_GetObjectItem(d_item, "heartbeat_interval") : NULL;
            if (cJSON_IsNumber(heartbeat)) {
                s_heartbeat_interval_ms = heartbeat->valueint;
            }

            ESP_LOGI(TAG, "QQ gateway HELLO, heartbeat=%dms", s_heartbeat_interval_ms);

            if (s_session_id[0] != '\0' && s_last_seq >= 0) {
                qq_send_resume_frame();
            } else {
                qq_send_identify_frame();
            }
            break;
        }

        case 0:
            if (event_type && strcmp(event_type, "READY") == 0) {
                const char *session_id = d_item ? cJSON_GetStringValue(cJSON_GetObjectItem(d_item, "session_id")) : NULL;
                if (session_id) {
                    snprintf(s_session_id, sizeof(s_session_id), "%s", session_id);
                }
                s_connected = true;
                ESP_LOGI(TAG, "QQ gateway READY, session established");
            } else if (event_type && strcmp(event_type, "RESUMED") == 0) {
                s_connected = true;
                ESP_LOGI(TAG, "QQ gateway RESUMED");
            } else {
                ESP_LOGD(TAG, "QQ dispatch event: %s", event_type ? event_type : "(null)");
                qq_handle_dispatch_event(event_type, d_item);
            }
            break;

        case 7:
            ESP_LOGW(TAG, "QQ gateway requested reconnect");
            s_need_reconnect = true;
            break;

        case 9:
            if (!cJSON_IsTrue(d_item)) {
                s_session_id[0] = '\0';
                s_last_seq = -1;
            }
            ESP_LOGW(TAG, "QQ gateway invalid session, reset=%s", cJSON_IsTrue(d_item) ? "no" : "yes");
            s_need_reconnect = true;
            break;

        case 11:
            ESP_LOGD(TAG, "QQ gateway heartbeat ACK");
        default:
            break;
        }

        cJSON_Delete(root);
        break;
    }

    default:
        break;
    }
}

static void qq_ws_task(void *arg)
{
    (void)arg;

    while (1) {
        esp_websocket_client_config_t ws_cfg = {0};

        if (!ensure_access_token()) {
            ESP_LOGW(TAG, "QQ startup blocked: failed to obtain access token");
            vTaskDelay(pdMS_TO_TICKS(EC_QQ_RECONNECT_MS));
            continue;
        }

        if (!fetch_gateway_url()) {
            ESP_LOGW(TAG, "QQ startup blocked: failed to obtain gateway url");
            vTaskDelay(pdMS_TO_TICKS(EC_QQ_RECONNECT_MS));
            continue;
        }

        s_need_reconnect = false;
        s_connected = false;
        s_heartbeat_interval_ms = 0;
        s_last_heartbeat_us = 0;

        ws_cfg.uri = s_gateway_url;
        ws_cfg.buffer_size = 2048;
        ws_cfg.network_timeout_ms = EC_QQ_HTTP_TIMEOUT_MS;
        ws_cfg.task_stack = EC_QQ_TASK_STACK;
        ws_cfg.task_prio = EC_QQ_TASK_PRIO;
        ws_cfg.task_name = "qq_ws";
        ws_cfg.reconnect_timeout_ms = EC_QQ_RECONNECT_MS;
        ws_cfg.crt_bundle_attach = esp_crt_bundle_attach;

        ESP_LOGI(TAG, "Connecting QQ gateway websocket: %.120s", s_gateway_url);
        s_ws_client = esp_websocket_client_init(&ws_cfg);
        if (!s_ws_client) {
            ESP_LOGW(TAG, "QQ websocket init failed");
            vTaskDelay(pdMS_TO_TICKS(EC_QQ_RECONNECT_MS));
            continue;
        }

        if (esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY, handle_ws_event, NULL) != ESP_OK ||
            esp_websocket_client_start(s_ws_client) != ESP_OK) {
            ESP_LOGW(TAG, "QQ websocket start failed");
            esp_websocket_client_destroy(s_ws_client);
            s_ws_client = NULL;
            vTaskDelay(pdMS_TO_TICKS(EC_QQ_RECONNECT_MS));
            continue;
        }

        while (!s_need_reconnect) {
            if (s_ws_client && esp_websocket_client_is_connected(s_ws_client) && s_heartbeat_interval_ms > 0) {
                int64_t now = esp_timer_get_time();
                if (now - s_last_heartbeat_us >= (int64_t)s_heartbeat_interval_ms * 1000LL) {
                    qq_send_heartbeat();
                }
            }
            vTaskDelay(pdMS_TO_TICKS(EC_QQ_WS_LOOP_DELAY_MS));
        }

        if (s_ws_client) {
            ESP_LOGI(TAG, "Closing QQ websocket and preparing reconnect");
            esp_websocket_client_stop(s_ws_client);
            esp_websocket_client_destroy(s_ws_client);
            s_ws_client = NULL;
        }
        s_connected = false;
        vTaskDelay(pdMS_TO_TICKS(EC_QQ_RECONNECT_MS));
    }
}

static esp_err_t ec_channel_qq_start(void)
{
    if (EC_QQ_APP_ID[0] == '\0' || EC_QQ_CLIENT_SECRET[0] == '\0') {
        ESP_LOGE(TAG, "QQ AppID/clientSecret not configured");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ws_task) {
        return ESP_OK;
    }

    if (xTaskCreate(qq_ws_task, "qq_gateway", EC_QQ_TASK_STACK, NULL, EC_QQ_TASK_PRIO, &s_ws_task) != pdPASS) {
        s_ws_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static esp_err_t ec_channel_qq_send(const ec_msg_t *msg)
{
    if (!msg) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!qq_post_message(msg)) {
        return ESP_FAIL;
    }

    return ESP_OK;
}


#endif