/**
 * @file ec_channel_ws.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief 
 * @version 0.1
 * @date 2026-03-06
 * 
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 * 
 */

/* ==================== [Includes] ========================================== */

#include "ec_config_internal.h"

#if EC_WS_ENABLE

#include "core/ec_channel.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

#include "core/ec_agent.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct {
    int fd;
    char chat_id[32];
    char chat_type[16];
    bool active;
} ws_client_t;


/* ==================== [Static Prototypes] ================================= */

static ws_client_t *find_client_by_fd(int fd);
static ws_client_t *find_client_for_msg(const ec_msg_t *msg);
static ws_client_t *add_client(int fd);
static void remove_client(int fd);
static esp_err_t ws_handler(httpd_req_t *req);
static esp_err_t ws_decode_message(int fd, const char *payload_json, ec_msg_t *msg);
static char *ws_build_response_json_alloc(const ec_msg_t *msg);

static esp_err_t ec_channel_ws_start(void);
static esp_err_t ec_channel_ws_send(const ec_msg_t *msg);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "ws";

static httpd_handle_t s_server = NULL;

static ws_client_t s_clients[EC_WS_MAX_CLIENTS];
static const ec_channel_t s_driver = {
    .name = g_ec_channel_ws,
    .vtable = {
        .start = ec_channel_ws_start,
        .send = ec_channel_ws_send,
    }
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_channel_ws(void)
{
    ec_channel_register(&s_driver);

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static ws_client_t *find_client_by_fd(int fd)
{
    for (int i = 0; i < EC_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *find_client_for_msg(const ec_msg_t *msg)
{
    if (!msg || msg->chat_id[0] == '\0') {
        return NULL;
    }

    for (int i = 0; i < EC_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active || strcmp(s_clients[i].chat_id, msg->chat_id) != 0) {
            continue;
        }

        if (msg->chat_type[0] != '\0' && s_clients[i].chat_type[0] != '\0' &&
                strcmp(s_clients[i].chat_type, msg->chat_type) != 0) {
            continue;
        }

        return &s_clients[i];
    }
    return NULL;
}

static ws_client_t *add_client(int fd)
{
    for (int i = 0; i < EC_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            memset(&s_clients[i], 0, sizeof(s_clients[i]));
            s_clients[i].fd = fd;
            snprintf(s_clients[i].chat_id, sizeof(s_clients[i].chat_id), "ws_%d", fd);
            s_clients[i].active = true;
            ESP_LOGI(TAG, "Client connected: %s (fd=%d)", s_clients[i].chat_id, fd);
            return &s_clients[i];
        }
    }
    ESP_LOGW(TAG, "Max clients reached, rejecting fd=%d", fd);
    return NULL;
}

static void remove_client(int fd)
{
    for (int i = 0; i < EC_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            ESP_LOGI(TAG, "Client disconnected: %s", s_clients[i].chat_id);
            memset(&s_clients[i], 0, sizeof(s_clients[i]));
            return;
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* WebSocket handshake — register client */
        int fd = httpd_req_to_sockfd(req);
        add_client(fd);
        return ESP_OK;
    }

    /* Receive WebSocket frame */
    httpd_ws_frame_t ws_pkt = {0};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* Get frame length */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len == 0) return ESP_OK;

    ws_pkt.payload = calloc(1, ws_pkt.len + 1);
    if (!ws_pkt.payload) return ESP_ERR_NO_MEM;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        free(ws_pkt.payload);
        return ret;
    }

    int fd = httpd_req_to_sockfd(req);
    ec_msg_t msg = {0};
    esp_err_t decode_err = ws_decode_message(fd, (char *)ws_pkt.payload, &msg);
    free(ws_pkt.payload);

    if (decode_err == ESP_OK) {
        esp_err_t push_err = ec_agent_inbound(&msg);
        if (push_err != ESP_OK) {
            ESP_LOGW(TAG, "Inbound queue full, drop ws message");
            free(msg.content);
        }
    } else if (decode_err == ESP_ERR_INVALID_ARG) {
        ESP_LOGW(TAG, "Invalid JSON from fd=%d", fd);
    }

    return ESP_OK;
}

static esp_err_t ec_channel_ws_start(void)
{
    memset(s_clients, 0, sizeof(s_clients));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = EC_WS_PORT;
    config.ctrl_port = EC_WS_PORT + 1;
    config.max_open_sockets = EC_WS_MAX_CLIENTS;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register WebSocket URI */
    httpd_uri_t ws_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    ESP_LOGI(TAG, "WebSocket server started on port %d", EC_WS_PORT);
    return ESP_OK;
}

static esp_err_t ec_channel_ws_send(const ec_msg_t *msg)
{
    if (!s_server) return ESP_ERR_INVALID_STATE;

    ws_client_t *client = find_client_for_msg(msg);
    if (!client) {
        ESP_LOGW(TAG, "No WS client with chat_type=%s chat_id=%s",
                 msg->chat_type, msg->chat_id);
        return ESP_ERR_NOT_FOUND;
    }

    char *json_str = ws_build_response_json_alloc(msg);
    if (!json_str) return ESP_ERR_NO_MEM;

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json_str,
        .len = strlen(json_str),
    };

    esp_err_t ret = httpd_ws_send_frame_async(s_server, client->fd, &ws_pkt);
    free(json_str);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send to %s: %s", msg->chat_id, esp_err_to_name(ret));
        remove_client(client->fd);
    } else {
        ESP_LOGI(TAG, "Sent WS response to %s (%d bytes)", msg->chat_id, (int)ws_pkt.len);
    }

    return ret;
}

static esp_err_t ws_decode_message(int fd, const char *payload_json, ec_msg_t *msg)
{
    ws_client_t *client;
    cJSON *root;
    cJSON *type;
    cJSON *content;
    cJSON *chan;
    cJSON *cid;
    cJSON *chat_type_item;
    const char *target_channel;
    const char *chat_id;
    const char *chat_type;

    if (!payload_json || !msg) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_Parse(payload_json);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    type = cJSON_GetObjectItem(root, "type");
    content = cJSON_GetObjectItem(root, "content");
    if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "message") != 0 ||
        !content || !cJSON_IsString(content)) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    client = find_client_by_fd(fd);
    chan = cJSON_GetObjectItem(root, "channel");
    target_channel = (chan && cJSON_IsString(chan) && chan->valuestring[0] != '\0')
                     ? chan->valuestring
                     : g_ec_channel_ws;

    cid = cJSON_GetObjectItem(root, "chat_id");
    chat_type_item = cJSON_GetObjectItem(root, "chat_type");
    if (strcmp(target_channel, g_ec_channel_ws) != 0) {
        if (!cid || !cJSON_IsString(cid) || !cid->valuestring || cid->valuestring[0] == '\0' ||
                !chat_type_item || !cJSON_IsString(chat_type_item) ||
                !chat_type_item->valuestring || chat_type_item->valuestring[0] == '\0') {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
    }

    chat_id = client ? client->chat_id : "ws_unknown";
    if (cid && cJSON_IsString(cid) && cid->valuestring && cid->valuestring[0] != '\0') {
        chat_id = cid->valuestring;
        if (client && strcmp(target_channel, g_ec_channel_ws) == 0) {
            strncpy(client->chat_id, chat_id, sizeof(client->chat_id) - 1);
        }
    }

    chat_type = client ? client->chat_type : "unknown";
    if (chat_type_item && cJSON_IsString(chat_type_item) &&
            chat_type_item->valuestring && chat_type_item->valuestring[0] != '\0') {
        chat_type = chat_type_item->valuestring;
        if (client && strcmp(target_channel, g_ec_channel_ws) == 0) {
            strncpy(client->chat_type, chat_type, sizeof(client->chat_type) - 1);
        }
    }

    ESP_LOGI(TAG, "WS message from %s (%s): %.40s...",
             chat_id, target_channel, content->valuestring);

    memset(msg, 0, sizeof(*msg));
    strncpy(msg->channel, target_channel, sizeof(msg->channel) - 1);
    strncpy(msg->chat_id, chat_id, sizeof(msg->chat_id) - 1);
    strncpy(msg->chat_type, chat_type, sizeof(msg->chat_type) - 1);
    msg->content = strdup(content->valuestring);
    cJSON_Delete(root);

    if (!msg->content) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static char *ws_build_response_json_alloc(const ec_msg_t *msg)
{
    cJSON *resp;
    char *json_str;

    if (!msg || !msg->content) {
        return NULL;
    }

    resp = cJSON_CreateObject();
    if (!resp) {
        return NULL;
    }

    cJSON_AddStringToObject(resp, "type", "response");
    cJSON_AddStringToObject(resp, "content", msg->content);
    cJSON_AddStringToObject(resp, "chat_id", msg->chat_id);
    cJSON_AddStringToObject(resp, "chat_type", msg->chat_type);

    json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    return json_str;
}

#endif
