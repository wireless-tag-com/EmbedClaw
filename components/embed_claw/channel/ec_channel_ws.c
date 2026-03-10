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

#include "ec_config.h"
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
    bool active;
} ws_client_t;


/* ==================== [Static Prototypes] ================================= */

static ws_client_t *find_client_by_fd(int fd);
static ws_client_t *find_client_by_chat_id(const char *chat_id);
static ws_client_t *add_client(int fd);
static void remove_client(int fd);
static esp_err_t ws_handler(httpd_req_t *req);

static esp_err_t ec_channel_ws_start(void);
static esp_err_t ec_channel_ws_send(const ec_msg_t *msg);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "ws";

static httpd_handle_t s_server = NULL;

static ws_client_t s_clients[EC_WS_MAX_CLIENTS];
static const ec_channel_t s_driver = {
    .name = EC_CHAN_WEBSOCKET,
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

static ws_client_t *find_client_by_chat_id(const char *chat_id)
{
    for (int i = 0; i < EC_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && strcmp(s_clients[i].chat_id, chat_id) == 0) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *add_client(int fd)
{
    for (int i = 0; i < EC_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
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
            s_clients[i].active = false;
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
    ws_client_t *client = find_client_by_fd(fd);

    /* Parse JSON message */
    cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    free(ws_pkt.payload);

    if (!root) {
        ESP_LOGW(TAG, "Invalid JSON from fd=%d", fd);
        return ESP_OK;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *content = cJSON_GetObjectItem(root, "content");

    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "message") == 0
        && content && cJSON_IsString(content)) {

        /* Optional channel: "feishu" = from Feishu long-connection relay (no public IP) */
        cJSON *chan = cJSON_GetObjectItem(root, "channel");
        bool is_feishu = (chan && cJSON_IsString(chan) && strcmp(chan->valuestring, "feishu") == 0);

        const char *chat_id = client ? client->chat_id : "ws_unknown";
        cJSON *cid = cJSON_GetObjectItem(root, "chat_id");
        if (cid && cJSON_IsString(cid)) {
            chat_id = cid->valuestring;
            if (client) {
                strncpy(client->chat_id, chat_id, sizeof(client->chat_id) - 1);
            }
        }
        if (is_feishu && (!cid || !cJSON_IsString(cid))) {
            ESP_LOGW(TAG, "Feishu relay message missing chat_id (open_id:xxx or chat_id:xxx)");
            cJSON_Delete(root);
            return ESP_OK;
        }

        ESP_LOGI(TAG, "WS message from %s (%s): %.40s...",
                 chat_id, is_feishu ? "feishu" : "ws", content->valuestring);
        // 将消息推送到消息总线，供Agent Loop处理
        ec_msg_t msg = {0};
        strncpy(msg.channel, is_feishu ? EC_CHAN_FEISHU : EC_CHAN_WEBSOCKET, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, chat_id, sizeof(msg.chat_id) - 1);
        msg.content = strdup(content->valuestring);
        if (msg.content) {
            esp_err_t push_err = ec_agent_inbound(&msg);
            if (push_err != ESP_OK) {
                ESP_LOGW(TAG, "Inbound queue full, drop ws message");
                free(msg.content);
            }
        }
    }

    cJSON_Delete(root);
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

    ws_client_t *client = find_client_by_chat_id(msg->chat_id);
    if (!client) {
        ESP_LOGW(TAG, "No WS client with chat_id=%s", msg->chat_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Build response JSON */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "response");
    cJSON_AddStringToObject(resp, "content", msg->content);
    cJSON_AddStringToObject(resp, "chat_id", msg->chat_id);

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

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
