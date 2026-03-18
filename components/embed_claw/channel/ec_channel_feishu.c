/**
 * @file ec_channel_feishu.c
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

#if EC_FEISHU_ENABLE

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_crt_bundle.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "esp_websocket_client.h"
#include "lwip/netdb.h"

#include "core/ec_channel.h"

/* ==================== [Defines] =========================================== */

#define EC_FEISHU_TOKEN_LEN       512
#define EC_FEISHU_AUTH_URL        "https://open.feishu.cn/open-apis/auth/v3/tenant_access_token/internal"
#define EC_FEISHU_MSG_URL         "https://open.feishu.cn/open-apis/im/v1/messages"
#define EC_FEISHU_ENDPOINT_URL    "https://open.feishu.cn/callback/ws/endpoint"

#define EC_FEISHU_TASK_STACK      (8 * 1024)
#define EC_FEISHU_TASK_PRIO       5
#define EC_FEISHU_TASK_CORE       0
#define EC_FEISHU_MAX_MSG_LEN     1800
#define EC_FEISHU_WS_URL_MAX      256
#define EC_FEISHU_WS_RECONNECT_MS 10000
#define EC_FEISHU_EVENT_QUEUE_LEN 16

#define EC_FEISHU_IGNORE_FIRST_N_MSGS          2
#define EC_FEISHU_LAST_EVENT_ID_N  32

/* ==================== [Typedefs] ========================================== */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} http_resp_t;

typedef struct {
    int32_t method;
    int32_t service;
    char type_val[32];
    char message_id[64];
    char trace_id[64];
    int sum, seq;
    uint64_t seq_id;
    uint64_t log_id;
    const uint8_t *payload;
    size_t payload_len;
    uint8_t header_blob[512];
    size_t header_blob_len;
} feishu_frame_t;

typedef struct {
    char *payload;
    size_t payload_len;
} feishu_event_job_t;

/* ==================== [Static Prototypes] ================================= */

static void http_resp_append(http_resp_t *r, const void *data, size_t len);
static esp_err_t http_event_handler(esp_http_client_event_t *evt);
static bool fetch_tenant_token(void);
static bool ensure_token(void);
static bool feishu_network_ready(void);
static bool fetch_ws_endpoint(void);
static int parse_varint(const uint8_t *buf, size_t len, size_t *consumed);
static size_t encoded_varint_size(uint64_t value);
static bool parse_feishu_frame(const uint8_t *buf, size_t len, feishu_frame_t *f);
static int encode_response_frame(uint8_t *out, size_t out_max, const feishu_frame_t *f);
static int encode_ping_frame(uint8_t *out, size_t out_max, int32_t service_id);
static void send_ws_ack(const feishu_frame_t *f);
static void process_ws_event_payload(const char *payload, size_t payload_len);
static void feishu_event_worker_task(void *arg);
static void handle_ws_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data);
static void feishu_ws_task(void *arg);
static void token_refresh_task(void *arg);

static esp_err_t ec_channel_feishu_start(void);
static esp_err_t ec_channel_feishu_send(const ec_msg_t *msg);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "feishu";

static char s_tenant_token[EC_FEISHU_TOKEN_LEN] = "";
static int64_t s_token_expire_at_us = 0;
static esp_websocket_client_handle_t s_ws_client = NULL;
static char s_ws_url[EC_FEISHU_WS_URL_MAX] = "";
static char s_service_id[24] = "";
static int s_ping_interval_s = EC_FEISHU_PING_INTERVAL_S;
static int64_t s_last_ping_us = 0;
static QueueHandle_t s_ws_event_queue = NULL;
static int64_t s_ws_connected_at_us = 0;
static char s_last_event_ids[EC_FEISHU_LAST_EVENT_ID_N][96] = {0};
static int s_last_event_id_idx = 0;

static const ec_channel_t s_driver = {
    .name = g_ec_channel_feishu,
    .vtable = {
        .start = ec_channel_feishu_start,
        .send = ec_channel_feishu_send,
    }
};

/* ==================== [Macros] ============================================ */

#define EC_FEISHU_W_VARINT(out, out_max, off, v) do { \
    uint64_t _x = (v); \
    while (_x >= 0x80) { \
        if ((off) >= (out_max)) return -1; \
        (out)[(off)++] = (uint8_t)(_x | 0x80); \
        _x >>= 7; \
    } \
    if ((off) >= (out_max)) return -1; \
    (out)[(off)++] = (uint8_t)_x; \
} while (0)

/* ==================== [Global Functions] ================================== */

esp_err_t ec_channel_feishu(void)
{
    ec_channel_register(&s_driver);

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static void http_resp_append(http_resp_t *r, const void *data, size_t len)
{
    if (r->len + len + 1 > r->cap) {
        size_t new_cap = r->cap ? r->cap * 2 : 4096;
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

/* Get tenant_access_token; updates s_tenant_token and s_token_expire_at_us */
static bool fetch_tenant_token(void)
{
    cJSON *body = cJSON_CreateObject();
    if (!body) {
        return false;
    }
    cJSON_AddStringToObject(body, "app_id", EC_SECRET_FEISHU_APP_ID);
    cJSON_AddStringToObject(body, "app_secret", EC_SECRET_FEISHU_APP_SECRET);
    char *json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!json_str) {
        return false;
    }

    http_resp_t resp = { .buf = NULL, .len = 0, .cap = 0 };
    resp.buf = calloc(1, 2048);
    if (!resp.buf) {
        free(json_str);
        return false;
    }
    resp.cap = 2048;

    esp_http_client_config_t config = {
        .url = EC_FEISHU_AUTH_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = 10000,
        .buffer_size = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(resp.buf);
        free(json_str);
        return false;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    free(json_str);

    bool ok = false;
    if (err == ESP_OK && resp.buf) {
        cJSON *root = cJSON_Parse(resp.buf);
        if (root) {
            cJSON *code = cJSON_GetObjectItem(root, "code");
            if (cJSON_IsNumber(code) && code->valueint == 0) {
                cJSON *tok = cJSON_GetObjectItem(root, "tenant_access_token");
                cJSON *exp = cJSON_GetObjectItem(root, "expire");
                if (cJSON_IsString(tok) && cJSON_IsNumber(exp)) {
                    strncpy(s_tenant_token, tok->valuestring, EC_FEISHU_TOKEN_LEN - 1);
                    s_tenant_token[EC_FEISHU_TOKEN_LEN - 1] = '\0';
                    int64_t expire_s = (int64_t)exp->valuedouble;
                    s_token_expire_at_us = esp_timer_get_time() + expire_s * 1000000LL;
                    ok = true;
                    ESP_LOGI(TAG, "Tenant token obtained, expire in %lld s", (long long)expire_s);
                }
            } else {
                cJSON *msg = cJSON_GetObjectItem(root, "msg");
                ESP_LOGE(TAG, "Auth failed: %s", msg && cJSON_IsString(msg) ? msg->valuestring : "unknown");
            }
            cJSON_Delete(root);
        }
    }
    free(resp.buf);
    return ok;
}

static bool ensure_token(void)
{
    if (!feishu_network_ready()) {
        return false;
    }
    int64_t now = esp_timer_get_time();
    if (s_tenant_token[0] && now < s_token_expire_at_us - 600000000LL) { /* 10 min margin */
        return true;
    }
    return fetch_tenant_token();
}

static bool feishu_network_ready(void)
{
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta) {
        return false;
    }

    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(sta, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        return false;
    }

    struct addrinfo hints = {0};
    struct addrinfo *res = NULL;
    hints.ai_family = AF_UNSPEC;
    int rc = getaddrinfo("open.feishu.cn", NULL, &hints, &res);
    if (rc != 0 || !res) {
        return false;
    }

    freeaddrinfo(res);
    return true;
}


/* Fetch WebSocket endpoint URL: POST open.feishu.cn/callback/ws/endpoint with AppID/AppSecret */
static bool fetch_ws_endpoint(void)
{
    cJSON *body = cJSON_CreateObject();
    if (!body) {
        return false;
    }
    cJSON_AddStringToObject(body, "AppID", EC_SECRET_FEISHU_APP_ID);
    cJSON_AddStringToObject(body, "AppSecret", EC_SECRET_FEISHU_APP_SECRET);
    cJSON_AddStringToObject(body, "app_id", EC_SECRET_FEISHU_APP_ID);
    cJSON_AddStringToObject(body, "app_secret", EC_SECRET_FEISHU_APP_SECRET);
    char *json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    if (!json_str) {
        return false;
    }

    http_resp_t resp = { .buf = NULL, .len = 0, .cap = 0 };
    resp.buf = calloc(1, 2048);
    if (!resp.buf) {
        free(json_str);
        return false;
    }
    resp.cap = 2048;

    bool ok = false;

    const char *endpoint_url = EC_FEISHU_ENDPOINT_URL;
    resp.len = 0;
    resp.buf[0] = '\0';

    esp_http_client_config_t config = {
        .url = endpoint_url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = 15000,
        .buffer_size = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(json_str);
        free(resp.buf);
        return false;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "locale", "zh");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Endpoint request failed (%s): %s", endpoint_url, esp_err_to_name(err));
        free(json_str);
        free(resp.buf);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.buf);
    if (!root) {
        ESP_LOGW(TAG, "Endpoint parse failed (%s): status=%d body=%.240s", endpoint_url, status, resp.buf);
        free(json_str);
        free(resp.buf);
        return false;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code) {
        code = cJSON_GetObjectItem(root, "Code");
    }
    int code_val = -1;
    if (cJSON_IsNumber(code)) {
        code_val = code->valueint;
    } else if (cJSON_IsString(code) && code->valuestring) {
        code_val = atoi(code->valuestring);
    }

    if (code_val == 0) {
        cJSON *data = cJSON_GetObjectItem(root, "data");
        if (!data) {
            data = cJSON_GetObjectItem(root, "Data");
        }
        if (data) {
            cJSON *url_node = cJSON_GetObjectItem(data, "URL");
            if (!url_node) {
                url_node = cJSON_GetObjectItem(data, "url");
            }
            if (!url_node) {
                url_node = cJSON_GetObjectItem(data, "WsURL");
            }
            if (!url_node) {
                url_node = cJSON_GetObjectItem(data, "ws_url");
            }
            if (!url_node) {
                url_node = cJSON_GetObjectItem(root, "URL");
            }
            if (!url_node) {
                url_node = cJSON_GetObjectItem(root, "url");
            }
            if (cJSON_IsString(url_node) && url_node->valuestring) {
                strncpy(s_ws_url, url_node->valuestring, EC_FEISHU_WS_URL_MAX - 1);
                s_ws_url[EC_FEISHU_WS_URL_MAX - 1] = '\0';
                const char *q = strchr(s_ws_url, '?');
                if (q) {
                    const char *sid = strstr(q, "service_id=");
                    if (sid) {
                        sid += 11;
                        size_t i = 0;
                        while (i < sizeof(s_service_id) - 1 && sid[i] && sid[i] != '&') {
                            s_service_id[i] = sid[i];
                            i++;
                        }
                        s_service_id[i] = '\0';
                    }
                    /* Fallback: some endpoint URLs only have fpid= (e.g. ...?fpid=493&aid=...); use as service_id for ping */
                    if (s_service_id[0] == '\0') {
                        const char *fpid = strstr(q, "fpid=");
                        if (fpid) {
                            fpid += 5;
                            size_t i = 0;
                            while (i < sizeof(s_service_id) - 1 && fpid[i] && fpid[i] != '&') {
                                s_service_id[i] = fpid[i];
                                i++;
                            }
                            s_service_id[i] = '\0';
                        }
                    }
                }
                cJSON *cfg = cJSON_GetObjectItem(data, "ClientConfig");
                if (!cfg) {
                    cfg = cJSON_GetObjectItem(data, "client_config");
                }
                if (cfg) {
                    cJSON *pi = cJSON_GetObjectItem(cfg, "PingInterval");
                    if (!pi) {
                        pi = cJSON_GetObjectItem(cfg, "ping_interval");
                    }
                    if (cJSON_IsNumber(pi) && pi->valueint > 0) {
                        s_ping_interval_s = pi->valueint;
                    }
                }
                ok = true;
                ESP_LOGI(TAG, "WS endpoint from %s: %.80s", endpoint_url, s_ws_url);
            }
        }
    } else {
        cJSON *msg = cJSON_GetObjectItem(root, "msg");
        if (!msg) {
            msg = cJSON_GetObjectItem(root, "Msg");
        }
        if (!msg) {
            msg = cJSON_GetObjectItem(root, "message");
        }
        ESP_LOGW(TAG, "Endpoint rejected (%s): code=%d msg=%s body=%.240s",
                 endpoint_url,
                 code_val,
                 msg && cJSON_IsString(msg) ? msg->valuestring : "unknown",
                 resp.buf);
    }
    cJSON_Delete(root);

    free(json_str);
    free(resp.buf);
    return ok;
}

/* Minimal protobuf: read varint, return value and bytes consumed */
static int parse_varint(const uint8_t *buf, size_t len, size_t *consumed)
{
    uint64_t val = 0;
    int shift = 0;
    *consumed = 0;
    while (*consumed < len && *consumed < 10) {
        uint8_t b = buf[(*consumed)++];
        val |= (uint64_t)(b & 0x7f) << shift;
        if ((b & 0x80) == 0) {
            return (int)val;
        }
        shift += 7;
    }
    return -1;
}

static size_t encoded_varint_size(uint64_t value)
{
    size_t size = 1;

    while (value >= 0x80) {
        value >>= 7;
        size++;
    }

    return size;
}

static bool parse_feishu_frame(const uint8_t *buf, size_t len, feishu_frame_t *f)
{
    memset(f, 0, sizeof(*f));
    f->method = -1;
    size_t off = 0;
    while (off < len) {
        if (len - off < 2) {
            break;
        }
        size_t tag_len = 0;
        int tag = parse_varint(buf + off, len - off, &tag_len);
        if (tag < 0) {
            break;
        }
        off += tag_len;
        int wire = tag & 7;
        int field = tag >> 3;
        if (wire == 0) {
            int v = parse_varint(buf + off, len - off, &tag_len);
            if (v < 0) {
                break;
            }
            off += tag_len;
            if (field == 1) {
                f->seq_id = (uint64_t)v;
            } else if (field == 2) {
                f->log_id = (uint64_t)v;
            } else if (field == 3) {
                f->service = (int32_t)v;
            } else if (field == 4) {
                f->method = (int32_t)v;
            }
        } else if (wire == 2) {
            int L = parse_varint(buf + off, len - off, &tag_len);
            if (L < 0 || off + tag_len + L > len) {
                break;
            }
            off += tag_len;
            const uint8_t *sub = buf + off;
            size_t sub_len = (size_t)L;
            off += L;
            if (field == 5) {
                f->header_blob_len = (sub_len < sizeof(f->header_blob)) ? sub_len : sizeof(f->header_blob);
                memcpy(f->header_blob, sub, f->header_blob_len);
                size_t hi = 0;
                while (hi < sub_len) {
                    if (sub_len - hi < 2) {
                        break;
                    }
                    int ht = parse_varint(sub + hi, sub_len - hi, &tag_len);
                    if (ht < 0) {
                        break;
                    }
                    hi += tag_len;
                    int hwire = ht & 7, hfield = ht >> 3;
                    if (hwire == 2) {
                        int Lk = parse_varint(sub + hi, sub_len - hi, &tag_len);
                        if (Lk < 0 || hi + tag_len + Lk > sub_len) {
                            break;
                        }
                        hi += tag_len;
                        if (hfield == 1 && Lk < 32) {
                            char key[32];
                            memcpy(key, sub + hi, Lk);
                            key[Lk] = '\0';
                            hi += Lk;
                            if (sub_len - hi < 2) {
                                break;
                            }
                            size_t vt_len = 0;
                            int hv = parse_varint(sub + hi, sub_len - hi, &vt_len);
                            if ((hv & 7) != 2) {
                                break;
                            }
                            hi += vt_len;
                            if (sub_len - hi < 1) {
                                break;
                            }
                            int Lv = parse_varint(sub + hi, sub_len - hi, &tag_len);
                            if (Lv < 0 || hi + tag_len + (size_t)Lv > sub_len) {
                                break;
                            }
                            hi += tag_len;
                            if (strcmp(key, "type") == 0 && Lv < (int)sizeof(f->type_val)) {
                                memcpy(f->type_val, sub + hi, Lv), f->type_val[Lv] = '\0';
                            } else if (strcmp(key, "message_id") == 0 && Lv < (int)sizeof(f->message_id)) {
                                memcpy(f->message_id, sub + hi, Lv), f->message_id[Lv] = '\0';
                            } else if (strcmp(key, "trace_id") == 0 && Lv < (int)sizeof(f->trace_id)) {
                                memcpy(f->trace_id, sub + hi, Lv), f->trace_id[Lv] = '\0';
                            } else if (strcmp(key, "sum") == 0 && Lv < 8) {
                                char tmp[8];
                                memcpy(tmp, sub + hi, Lv);
                                tmp[Lv] = '\0';
                                f->sum = atoi(tmp);
                            } else if (strcmp(key, "seq") == 0 && Lv < 8) {
                                char tmp[8];
                                memcpy(tmp, sub + hi, Lv);
                                tmp[Lv] = '\0';
                                f->seq = atoi(tmp);
                            }
                            hi += (size_t)Lv;
                        }
                    }
                }
            } else if (field == 8) {
                f->payload = sub;
                f->payload_len = sub_len;
            }
        }
    }
    return f->method >= 0;
}

/* Encode response frame (Method=1, same Service/SeqID/LogID/Headers + biz_rt, Payload={"code":200}) */
static int encode_response_frame(uint8_t *out, size_t out_max, const feishu_frame_t *f)
{
    const char *resp_json = "{\"code\":200}";
    const char *biz_rt_key = "biz_rt";
    size_t resp_len = strlen(resp_json);
    size_t biz_rt_key_len = strlen(biz_rt_key);
    size_t off = 0;
    int rt = (int)(esp_timer_get_time() / 1000);
    char rt_str[12];
    int rt_len = snprintf(rt_str, sizeof(rt_str), "%d", rt);
    size_t biz_rt_value_len = (size_t)rt_len;
    size_t biz_rt_header_len = 1 + encoded_varint_size(biz_rt_key_len) + biz_rt_key_len +
                               1 + encoded_varint_size(biz_rt_value_len) + biz_rt_value_len;
    size_t total_header_len = f->header_blob_len + biz_rt_header_len;

    EC_FEISHU_W_VARINT(out, out_max, off, 1 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, f->seq_id);
    EC_FEISHU_W_VARINT(out, out_max, off, 2 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, f->log_id);
    EC_FEISHU_W_VARINT(out, out_max, off, 3 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, (uint64_t)f->service);
    EC_FEISHU_W_VARINT(out, out_max, off, 4 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, 1);
    EC_FEISHU_W_VARINT(out, out_max, off, 5 << 3 | 2); EC_FEISHU_W_VARINT(out, out_max, off, total_header_len);
    if (off + total_header_len > out_max) {
        return -1;
    }
    memcpy(out + off, f->header_blob, f->header_blob_len);
    off += f->header_blob_len;
    out[off++] = 0x0a; EC_FEISHU_W_VARINT(out, out_max, off, biz_rt_key_len);
    memcpy(out + off, biz_rt_key, biz_rt_key_len); off += biz_rt_key_len;
    out[off++] = 0x12; EC_FEISHU_W_VARINT(out, out_max, off, biz_rt_value_len);
    memcpy(out + off, rt_str, biz_rt_value_len); off += biz_rt_value_len;
    EC_FEISHU_W_VARINT(out, out_max, off, 8 << 3 | 2); EC_FEISHU_W_VARINT(out, out_max, off, resp_len);
    if (off + resp_len > out_max) {
        return -1;
    }
    memcpy(out + off, resp_json, resp_len);
    off += resp_len;
    return (int)off;
}

/* Encode ping frame (Method=0 Control, Service=service_id, Headers type=ping) */
static int encode_ping_frame(uint8_t *out, size_t out_max, int32_t service_id)
{
    size_t off = 0;
    const char *type_key = "type";
    const char *ping_val = "ping";
    size_t type_key_len = strlen(type_key);
    size_t ping_val_len = strlen(ping_val);
    size_t header_len = 1 + encoded_varint_size(type_key_len) + type_key_len +
                        1 + encoded_varint_size(ping_val_len) + ping_val_len;
    EC_FEISHU_W_VARINT(out, out_max, off, 1 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, 0);
    EC_FEISHU_W_VARINT(out, out_max, off, 2 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, 0);
    EC_FEISHU_W_VARINT(out, out_max, off, 3 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, (uint64_t)service_id);
    EC_FEISHU_W_VARINT(out, out_max, off, 4 << 3 | 0); EC_FEISHU_W_VARINT(out, out_max, off, 0);
    EC_FEISHU_W_VARINT(out, out_max, off, 5 << 3 | 2); EC_FEISHU_W_VARINT(out, out_max, off, header_len);
    if (off + header_len > out_max) {
        return -1;
    }
    out[off++] = 0x0a; EC_FEISHU_W_VARINT(out, out_max, off, type_key_len);
    memcpy(out + off, type_key, type_key_len); off += type_key_len;
    out[off++] = 0x12; EC_FEISHU_W_VARINT(out, out_max, off, ping_val_len);
    memcpy(out + off, ping_val, ping_val_len); off += ping_val_len;
    return (int)off;
}

static void send_ws_ack(const feishu_frame_t *f)
{
    if (!f || !s_ws_client) {
        return;
    }
    uint8_t resp_buf[512];
    int n = encode_response_frame(resp_buf, sizeof(resp_buf), f);
    if (n > 0) {
        esp_websocket_client_send_bin(s_ws_client, (char *)resp_buf, n, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "Sent WS ACK (seq_id=%" PRIu64 " log_id=%" PRIu64 " service=%d method=%d type=%s message_id=%s trace_id=%s)",
             f->seq_id, f->log_id, f->service, f->method, f->type_val, f->message_id, f->trace_id);
}

static void process_ws_event_payload(const char *payload, size_t payload_len)
{
    if (!payload || payload_len == 0) {
        return;
    }

    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        return;
    }

    char *text = NULL;
    char reply_chat_type[16] = {0};
    char reply_chat_id[64] = {0};

    cJSON *header = cJSON_GetObjectItem(root, "header");
    cJSON *event = cJSON_GetObjectItem(root, "event");
    if (!header || !event) {
        ESP_LOGI(TAG, "Feishu WS: no header/event in payload");
        goto ws_data_done;
    }

    cJSON *ev_type = cJSON_GetObjectItem(header, "event_type");
    const char *ev_type_str = (ev_type && cJSON_IsString(ev_type)) ? ev_type->valuestring : "(null)";
    ESP_LOGI(TAG, "Feishu WS: event_type=%s", ev_type_str);
    if (!cJSON_IsString(ev_type) || strcmp(ev_type->valuestring, "im.message.receive_v1") != 0) {
        goto ws_data_done;
    }

    /* Log full payload in async task to avoid blocking websocket callback */
    ESP_LOGI(TAG, "Feishu WS: full payload (%u bytes):", (unsigned)payload_len);
    ESP_LOGI(TAG, "%.*s", (int)payload_len, payload);

    cJSON *sender = cJSON_GetObjectItem(event, "sender");
    cJSON *message = cJSON_GetObjectItem(event, "message");
    if (!sender || !message) {
        ESP_LOGI(TAG, "Feishu WS: event missing sender or message");
        goto ws_data_done;
    }

    cJSON *sender_type = cJSON_GetObjectItem(sender, "sender_type");
    const char *st = (sender_type && cJSON_IsString(sender_type)) ? sender_type->valuestring : "(missing)";
    cJSON *sender_id = cJSON_GetObjectItem(sender, "sender_id");
    cJSON *open_id_node = sender_id ? cJSON_GetObjectItem(sender_id, "open_id") : NULL;
    const char *oid = (open_id_node && cJSON_IsString(open_id_node)) ? open_id_node->valuestring : "(missing)";
    char *sender_json = cJSON_PrintUnformatted(sender);
    if (sender_json) {
        ESP_LOGI(TAG, "Feishu WS: sender.sender_type=%s open_id=%s sender_json=%.200s", st, oid, sender_json);
        free(sender_json);
    } else {
        ESP_LOGI(TAG, "Feishu WS: sender.sender_type=%s open_id=%s", st, oid);
    }

    if (sender_type && cJSON_IsString(sender_type) && strcmp(sender_type->valuestring, "app") == 0) {
        ESP_LOGI(TAG, "Feishu WS: filtered (sender_type=app), not pushing to agent");
        goto ws_data_done;
    }

    // 通过event_id去重，避免飞书的重试机制导致消息重复推送给agent
    cJSON *event_id_node = cJSON_GetObjectItem(header, "event_id");
    if (!event_id_node) {
        event_id_node = cJSON_GetObjectItem(root, "event_id");
    }
    const char *event_id = (event_id_node && cJSON_IsString(event_id_node)) ? event_id_node->valuestring : NULL;
    if (event_id && event_id[0]) {
        int i;
        for (i = 0; i < EC_FEISHU_LAST_EVENT_ID_N; i++) {
            if (s_last_event_ids[i][0] && strcmp(s_last_event_ids[i], event_id) == 0) {
                ESP_LOGI(TAG, "Feishu WS: duplicate event_id=%.48s (resend/replay), not pushing", event_id);
                goto ws_data_done;
            }
        }
        strncpy(s_last_event_ids[s_last_event_id_idx], event_id, sizeof(s_last_event_ids[0]) - 1);
        s_last_event_ids[s_last_event_id_idx][sizeof(s_last_event_ids[0]) - 1] = '\0';
        s_last_event_id_idx = (s_last_event_id_idx + 1) % EC_FEISHU_LAST_EVENT_ID_N;
    }


    cJSON *chat_id_node = cJSON_GetObjectItem(message, "chat_id");
    cJSON *chat_type_node = cJSON_GetObjectItem(message, "chat_type");
    cJSON *content_node = cJSON_GetObjectItem(message, "content");
    char open_id_buf[48] = {0};
    char chat_id_buf[48] = {0};
    char chat_type_buf[16] = "p2p";
    if (open_id_node && cJSON_IsString(open_id_node) && open_id_node->valuestring) {
        strncpy(open_id_buf, open_id_node->valuestring, sizeof(open_id_buf) - 1);
    }
    if (chat_id_node && cJSON_IsString(chat_id_node) && chat_id_node->valuestring) {
        strncpy(chat_id_buf, chat_id_node->valuestring, sizeof(chat_id_buf) - 1);
    }
    if (chat_type_node && cJSON_IsString(chat_type_node) && chat_type_node->valuestring) {
        strncpy(chat_type_buf, chat_type_node->valuestring, sizeof(chat_type_buf) - 1);
    }
    const char *content_str = content_node && cJSON_IsString(content_node) ? content_node->valuestring : NULL;
    if (!content_str) {
        goto ws_data_done;
    }

    cJSON *content_json = cJSON_Parse(content_str);
    if (content_json) {
        cJSON *t = cJSON_GetObjectItem(content_json, "text");
        if (cJSON_IsString(t) && t->valuestring) {
            text = strdup(t->valuestring);
        }
        cJSON_Delete(content_json);
    }
    if (!text) {
        text = strdup(content_str);
    }
    if (!text) {
        goto ws_data_done;
    }

    if (strcmp(chat_type_buf, "p2p") == 0 && open_id_buf[0]) {
        snprintf(reply_chat_id, sizeof(reply_chat_id), "%s", open_id_buf);
        snprintf(reply_chat_type, sizeof(reply_chat_type), "open_id");
    } else if (chat_id_buf[0]) {
        snprintf(reply_chat_id, sizeof(reply_chat_id), "%s", chat_id_buf);
        snprintf(reply_chat_type, sizeof(reply_chat_type), "chat_id");
    } else if (open_id_buf[0]) {
        snprintf(reply_chat_id, sizeof(reply_chat_id), "%s", open_id_buf);
        snprintf(reply_chat_type, sizeof(reply_chat_type), "open_id");
    } else {
        goto ws_data_done;
    }

    ec_msg_t msg = {0};
    strncpy(msg.channel, g_ec_channel_feishu, sizeof(msg.channel) - 1);
    strncpy(msg.chat_type, reply_chat_type, sizeof(msg.chat_type) - 1);
    strncpy(msg.chat_id, reply_chat_id, sizeof(msg.chat_id) - 1);
    msg.content = text;
    ESP_LOGI(TAG, "Feishu WS: pushing to agent chat_id=%s content_len=%d", reply_chat_id, (int)strlen(text));
    if (ec_agent_inbound(&msg) != ESP_OK) {
        ESP_LOGW(TAG, "Feishu WS: inbound queue full, drop message");
        free(msg.content);
    }

    text = NULL;

ws_data_done:
    cJSON_Delete(root);
    free(text);
}

static void feishu_event_worker_task(void *arg)
{
    (void)arg;
    feishu_event_job_t job = {0};
    while (1) {
        if (xQueueReceive(s_ws_event_queue, &job, portMAX_DELAY) == pdTRUE) {
            process_ws_event_payload(job.payload, job.payload_len);
            free(job.payload);
        }
    }
}

static void handle_ws_event(void *arg, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;

    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED: {
        int64_t now_us = esp_timer_get_time();
        ESP_LOGI(TAG, "Feishu WS connected");
        s_last_ping_us = now_us;
        s_ws_connected_at_us = now_us;
        break;
    }
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Feishu WS disconnected");
        break;
    case WEBSOCKET_EVENT_DATA: {
        if (!data || !data->data_len || data->op_code != 0x02) {
            break;
        }

        feishu_frame_t f;
        if (!parse_feishu_frame((const uint8_t *)data->data_ptr, data->data_len, &f)) {
            break;
        }

        /* Learn service_id from first server frame if URL had no service_id (e.g. only fpid) */
        if (f.service != 0 && s_service_id[0] == '\0') {
            (void)snprintf(s_service_id, sizeof(s_service_id), "%d", (int)f.service);
            ESP_LOGI(TAG, "Feishu WS: learned service_id=%s from server frame", s_service_id);
        }

        if (f.method == 0) {
            if (strcmp(f.type_val, "pong") == 0) {
                ESP_LOGD(TAG, "Feishu pong");
            } else {
                ESP_LOGI(TAG, "Feishu WS: control frame type=%s service=%d", f.type_val, (int)f.service);
            }
            break;
        }
        if (f.method != 1 || strcmp(f.type_val, "event") != 0) {
            break;
        }
        if (f.sum > 1 && f.seq != 0) {
            break;
        }
        if (f.payload_len == 0 || f.payload_len > 4096) {
            break;
        }

        send_ws_ack(&f);

        if (!s_ws_event_queue) {
            break;
        }
        char *payload_copy = malloc(f.payload_len + 1);
        if (!payload_copy) {
            ESP_LOGW(TAG, "Feishu WS: no mem for async payload");
            break;
        }
        memcpy(payload_copy, f.payload, f.payload_len);
        payload_copy[f.payload_len] = '\0';

        feishu_event_job_t job = {
            .payload = payload_copy,
            .payload_len = f.payload_len,
        };
        if (xQueueSend(s_ws_event_queue, &job, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Feishu WS: event queue full, drop payload");
            free(payload_copy);
        }
        break;
    }
    default:
        break;
    }
}

static void feishu_ws_task(void *arg)
{
    (void)arg;
    bool waiting_logged = false;

    while (1) {
        if (!EC_SECRET_FEISHU_APP_ID[0] || !EC_SECRET_FEISHU_APP_SECRET[0]) {
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }
        if (!feishu_network_ready()) {
            if (!waiting_logged) {
                ESP_LOGW(TAG, "Wi-Fi/IP/DNS not ready for Feishu, waiting...");
                waiting_logged = true;
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        waiting_logged = false;

        if (!fetch_ws_endpoint() || !s_ws_url[0]) {
            ESP_LOGW(TAG, "Feishu endpoint failed, retry in 60s");
            vTaskDelay(pdMS_TO_TICKS(60000));
            continue;
        }
        /* Fetch tenant token before connecting so first reply can be sent */
        if (!ensure_token()) {
            ESP_LOGW(TAG, "Feishu tenant token not ready, will retry on first send");
        }
        esp_websocket_client_config_t ws_cfg = {
            .uri = s_ws_url,
            .network_timeout_ms = 15000,
            .reconnect_timeout_ms = 5000,
            .buffer_size = 2048,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .task_stack = 6144,  /* keep enough stack for fast ACK path in callback */
        };
        s_ws_client = esp_websocket_client_init(&ws_cfg);
        if (!s_ws_client) {
            vTaskDelay(pdMS_TO_TICKS(EC_FEISHU_WS_RECONNECT_MS));
            continue;
        }
        esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY, handle_ws_event, NULL);
        esp_websocket_client_start(s_ws_client);
        int64_t last_ping = esp_timer_get_time();
        while (esp_websocket_client_is_connected(s_ws_client)) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            int64_t now = esp_timer_get_time();
            if ((now - last_ping) / 1000000 >= s_ping_interval_s) {
                int sid = atoi(s_service_id);
                uint8_t ping_buf[128];
                int n = encode_ping_frame(ping_buf, sizeof(ping_buf), sid);
                if (n > 0 && sid != 0) {
                    esp_websocket_client_send_bin(s_ws_client, (char *)ping_buf, n, portMAX_DELAY);
                    ESP_LOGD(TAG, "Feishu WS: sent ping service_id=%d", sid);
                } else if (sid == 0) {
                    ESP_LOGW(TAG, "Feishu WS: skip ping (no service_id yet)");
                }
                last_ping = now;
            }
        }
        esp_websocket_client_stop(s_ws_client);
        esp_websocket_client_destroy(s_ws_client);
        s_ws_client = NULL;
        vTaskDelay(pdMS_TO_TICKS(EC_FEISHU_WS_RECONNECT_MS));
    }
}

static void token_refresh_task(void *arg)
{
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); /* check every 1 min */
        if (EC_SECRET_FEISHU_APP_ID[0] && EC_SECRET_FEISHU_APP_SECRET[0]) {
            ensure_token();
        }
    }
}

static esp_err_t ec_channel_feishu_start(void)
{
    if (!s_ws_event_queue) {
        s_ws_event_queue = xQueueCreate(EC_FEISHU_EVENT_QUEUE_LEN, sizeof(feishu_event_job_t));
    }
    if (!s_ws_event_queue) {
        ESP_LOGE(TAG, "Failed to create Feishu event queue");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t evt_ok = xTaskCreatePinnedToCore(feishu_event_worker_task, "feishu_evt",
                        EC_FEISHU_TASK_STACK, NULL,
                        EC_FEISHU_TASK_PRIO, NULL, EC_FEISHU_TASK_CORE);
    if (evt_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Feishu event worker task");
        return ESP_FAIL;
    }

    BaseType_t tok_ok = xTaskCreatePinnedToCore(token_refresh_task, "feishu_tok",
                        EC_FEISHU_TASK_STACK, NULL,
                        EC_FEISHU_TASK_PRIO, NULL, EC_FEISHU_TASK_CORE);
    if (tok_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Feishu token task");
        return ESP_FAIL;
    }

    BaseType_t ws_ok = xTaskCreatePinnedToCore(feishu_ws_task, "feishu_ws",
                       EC_FEISHU_TASK_STACK, NULL,
                       EC_FEISHU_TASK_PRIO, NULL, EC_FEISHU_TASK_CORE);
    if (ws_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Feishu tasks");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Feishu long connection started (device connects to Feishu, no public IP)");
    return ESP_OK;
}

static esp_err_t ec_channel_feishu_send(const ec_msg_t *msg)
{
    if (!msg)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const char *chat_id = msg->chat_id;
    const char *chat_type = msg->chat_type;
    const char *text = msg->content;

    if (!chat_id || !chat_type || !text ||
            chat_id[0] == '\0' || chat_type[0] == '\0' || text[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!ensure_token()) {
        ESP_LOGE(TAG, "Failed to get tenant token");
        return ESP_FAIL;
    }

    /* Escape text for JSON: only " and \ need escape */
    size_t tlen = strlen(text);
    size_t chunk = (tlen > EC_FEISHU_MAX_MSG_LEN) ? EC_FEISHU_MAX_MSG_LEN : tlen;
    char *escaped = malloc(chunk * 2 + 32);
    if (!escaped) {
        return ESP_ERR_NO_MEM;
    }
    size_t j = 0;
    for (size_t i = 0; i < chunk && j < chunk * 2 + 28; i++) {
        char c = text[i];
        if (c == '"' || c == '\\') {
            escaped[j++] = '\\';
        }
        escaped[j++] = c;
    }
    escaped[j] = '\0';

    cJSON *content_inner = cJSON_CreateObject();
    cJSON_AddStringToObject(content_inner, "text", escaped);
    char *content_json = cJSON_PrintUnformatted(content_inner);
    cJSON_Delete(content_inner);
    free(escaped);
    if (!content_json) {
        return ESP_ERR_NO_MEM;
    }

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "receive_id", chat_id);
    cJSON_AddStringToObject(body, "msg_type", "text");
    cJSON_AddStringToObject(body, "content", content_json);
    char *body_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    free(content_json);
    if (!body_str) {
        return ESP_ERR_NO_MEM;
    }

    char url[128];
    snprintf(url, sizeof(url), "%s?receive_id_type=%s", EC_FEISHU_MSG_URL, chat_type);

    http_resp_t resp = { .buf = calloc(1, 2048), .len = 0, .cap = 2048 };
    if (!resp.buf) {
        free(body_str);
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = &resp,
        .timeout_ms = 10000,
        .buffer_size = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(resp.buf);
        free(body_str);
        return ESP_ERR_NO_MEM;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");
    char auth_hdr[EC_FEISHU_TOKEN_LEN + 16];
    snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", s_tenant_token);
    esp_http_client_set_header(client, "Authorization", auth_hdr);
    esp_http_client_set_post_field(client, body_str, strlen(body_str));

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    free(body_str);

    esp_err_t ret = ESP_FAIL;
    if (err == ESP_OK && resp.buf) {
        cJSON *root = cJSON_Parse(resp.buf);
        if (root) {
            cJSON *code = cJSON_GetObjectItem(root, "code");
            if (cJSON_IsNumber(code) && code->valueint == 0) {
                ret = ESP_OK;
                ESP_LOGI(TAG, "Feishu send ok to %s", chat_id);
            } else {
                cJSON *msg = cJSON_GetObjectItem(root, "msg");
                ESP_LOGE(TAG, "Feishu send failed: %s", msg && cJSON_IsString(msg) ? msg->valuestring : "unknown");
            }
            cJSON_Delete(root);
        }
    }

    free(resp.buf);
    return ret;
}

#endif
