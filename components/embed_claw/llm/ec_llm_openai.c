/**
 * @file ec_llm_openai.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-TAG. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "ec_llm_openai.h"
#include "esp_log.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "cJSON.h"
#include <string.h>

/* ==================== [Defines] =========================================== */

#define LLM_API_KEY_MAX_LEN 320
#define LLM_MODEL_MAX_LEN   64
#define LLM_DUMP_MAX_BYTES   (16 * 1024)
#define LLM_DUMP_CHUNK_BYTES 320

/* ==================== [Typedefs] ========================================== */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} resp_buf_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t ec_llm_openai_init(ec_llm_provider_t *self, const ec_llm_provider_ctx_t *provider_ctx);
static esp_err_t ec_llm_openai_chat_tools(ec_llm_provider_t *self, const char *system_prompt, cJSON *messages,
        const char *tools_json, ec_llm_response_t *resp);

static cJSON *convert_messages_openai(const char *system_prompt, cJSON *messages);
static void resp_buf_free(resp_buf_t *rb);
static esp_err_t llm_http(const char *post_data, resp_buf_t *rb, int *out_status);
static const char *select_server_ca_pem(const char *url);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "ec_llm_openai";

/*
 * DashScope currently presents a certificate chain rooted at GlobalSign Root
 * CA - R3. On some ESP32-C3 builds the crt bundle path can mis-verify this
 * chain, so prefer a pinned root CA for known aliyuncs.com endpoints.
 */
static const char *s_globalsign_root_ca_r3_pem =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDXzCCAkegAwIBAgILBAAAAAABIVhTCKIwDQYJKoZIhvcNAQELBQAwTDEgMB4G\n"
    "A1UECxMXR2xvYmFsU2lnbiBSb290IENBIC0gUjMxEzARBgNVBAoTCkdsb2JhbFNp\n"
    "Z24xEzARBgNVBAMTCkdsb2JhbFNpZ24wHhcNMDkwMzE4MTAwMDAwWhcNMjkwMzE4\n"
    "MTAwMDAwWjBMMSAwHgYDVQQLExdHbG9iYWxTaWduIFJvb3QgQ0EgLSBSMzETMBEG\n"
    "A1UEChMKR2xvYmFsU2lnbjETMBEGA1UEAxMKR2xvYmFsU2lnbjCCASIwDQYJKoZI\n"
    "hvcNAQEBBQADggEPADCCAQoCggEBAMwldpB5BngiFvXAg7aEyiie/QV2EcWtiHL8\n"
    "RgJDx7KKnQRfJMsuS+FggkbhUqsMgUdwbN1k0ev1LKMPgj0MK66X17YUhhB5uzsT\n"
    "gHeMCOFJ0mpiLx9e+pZo34knlTifBtc+ycsmWQ1z3rDI6SYOgxXG71uL0gRgykmm\n"
    "KPZpO/bLyCiR5Z2KYVc3rHQU3HTgOu5yLy6c+9C7v/U9AOEGM+iCK65TpjoWc4zd\n"
    "QQ4gOsC0p6Hpsk+QLjJg6VfLuQSSaGjlOCZgdbKfd/+RFO+uIEn8rUAVSNECMWEZ\n"
    "XriX7613t2Saer9fwRPvm2L7DWzgVGkWqQPabumDk3F2xmmFghcCAwEAAaNCMEAw\n"
    "DgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQFMAMBAf8wHQYDVR0OBBYEFI/wS3+o\n"
    "LkUkrk1Q+mOai97i3Ru8MA0GCSqGSIb3DQEBCwUAA4IBAQBLQNvAUKr+yAzv95ZU\n"
    "RUm7lgAJQayzE4aGKAczymvmdLm6AC2upArT9fHxD4q/c2dKg8dEe3jgr25sbwMp\n"
    "jjM5RcOO5LlXbKr8EpbsU8Yt5CRsuZRj+9xTaGdWPoO4zzUhw8lo/s7awlOqzJCK\n"
    "6fBdRoyV3XpYKBovHd7NADdBj+1EbddTKJd+82cEHhXXipa0095MJ6RMG3NzdvQX\n"
    "mcIfeg7jLQitChws/zyrVQ4PkX4268NXSb7hLi18YIvDQVETI53O9zJrlAGomecs\n"
    "Mx86OyXShkDOOyyGeMlhLxS67ttVb9+E7gUJTb0o2HLO02JQZR7rkpeDMdmztcpH\n"
    "WD9f\n"
    "-----END CERTIFICATE-----\n";

static ec_llm_provider_t  s_provider = {0};
static const ec_llm_provider_vtable_t ec_llm_openai_vtable = {
    .init = ec_llm_openai_init,
    .chat_tools = ec_llm_openai_chat_tools,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

ec_llm_provider_t  *ec_llm_openai_get_provider(void)
{
    s_provider.name = "openai";
    s_provider.instance.url = NULL;
    s_provider.instance.api_key = NULL;
    s_provider.instance.model = NULL;
    s_provider.vtable = &ec_llm_openai_vtable;

    return &s_provider;
}

/* ==================== [Static Functions] ================================== */

static cJSON *convert_tools_openai(const char *tools_json)
{
    if (!tools_json) {
        return NULL;
    }
    cJSON *arr = cJSON_Parse(tools_json);
    if (!arr || !cJSON_IsArray(arr)) {
        cJSON_Delete(arr);
        return NULL;
    }
    cJSON *out = cJSON_CreateArray();
    cJSON *tool;
    cJSON_ArrayForEach(tool, arr) {
        cJSON *name = cJSON_GetObjectItem(tool, "name");
        cJSON *desc = cJSON_GetObjectItem(tool, "description");
        cJSON *schema = cJSON_GetObjectItem(tool, "input_schema");
        if (!name || !cJSON_IsString(name)) {
            continue;
        }

        cJSON *func = cJSON_CreateObject();
        cJSON_AddStringToObject(func, "name", name->valuestring);
        if (desc && cJSON_IsString(desc)) {
            cJSON_AddStringToObject(func, "description", desc->valuestring);
        }
        if (schema) {
            cJSON_AddItemToObject(func, "parameters", cJSON_Duplicate(schema, 1));
        }

        cJSON *wrap = cJSON_CreateObject();
        cJSON_AddStringToObject(wrap, "type", "function");
        cJSON_AddItemToObject(wrap, "function", func);
        cJSON_AddItemToArray(out, wrap);
    }
    cJSON_Delete(arr);
    return out;
}


static esp_err_t ec_llm_openai_init(ec_llm_provider_t *self, const ec_llm_provider_ctx_t *provider_ctx)
{
    if (!self || !provider_ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    self->instance.url = provider_ctx->url;
    self->instance.api_key = provider_ctx->api_key;
    self->instance.model = provider_ctx->model;

    return ESP_OK;
}

static esp_err_t ec_llm_openai_chat_tools(ec_llm_provider_t *self, const char *system_prompt, cJSON *messages,
        const char *tools_json, ec_llm_response_t *resp)
{
    if (!self || !system_prompt || !messages || !tools_json || !resp) {
        ESP_LOGE(TAG, "Invalid arguments to ec_llm_openai_chat_tools");
        return ESP_ERR_INVALID_ARG;
    }

    if (!self->instance.api_key || self->instance.api_key[0] == '\0' ||
            !self->instance.model || self->instance.model[0] == '\0' ||
            !self->instance.url || self->instance.url[0] == '\0') {
        ESP_LOGE(TAG, "API key, model, and url must be provided");
        return ESP_ERR_INVALID_ARG;
    }

    memset(resp, 0, sizeof(ec_llm_response_t));

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "model", self->instance.model);

    /* OpenAI-style compatible chat completions (OpenAI & Qwen) */
    cJSON_AddNumberToObject(body, "max_tokens", EC_LLM_MAX_TOKENS);

    cJSON *openai_msgs = convert_messages_openai(system_prompt, messages);
    cJSON_AddItemToObject(body, "messages", openai_msgs);

    if (tools_json) {
        cJSON *tools = convert_tools_openai(tools_json);
        if (tools) {
            cJSON_AddItemToObject(body, "tools", tools);
            cJSON_AddStringToObject(body, "tool_choice", "auto");
        }
    }

    char *post_data = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    if (!post_data) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Calling LLM API with tools (model=%s, body=%u bytes)",
             self->instance.model, (unsigned)strlen(post_data));

    resp_buf_t rb = {
        .data = calloc(1, EC_LLM_STREAM_BUF_SIZE),
        .len = 0,
        .cap = EC_LLM_STREAM_BUF_SIZE,
    };
    if (!rb.data) {
        free(post_data);
        return ESP_ERR_NO_MEM;
    }

    int status = 0;
    esp_err_t err = llm_http(post_data, &rb, &status);
    free(post_data);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        resp_buf_free(&rb);
        return err;
    }

    if (status != 200) {
        /* 400 with content filter: return user-friendly message as response text */
        if (status == 400 && rb.data && rb.len > 0) {
            cJSON *root = cJSON_Parse(rb.data);
            if (root) {
                cJSON *err = cJSON_GetObjectItem(root, "error");
                if (err && cJSON_IsObject(err)) {
                    cJSON *code = cJSON_GetObjectItem(err, "code");
                    cJSON *msg = cJSON_GetObjectItem(err, "message");
                    const char *code_str = (code && cJSON_IsString(code)) ? code->valuestring : "";
                    const char *msg_str = (msg && cJSON_IsString(msg)) ? msg->valuestring : "";
                    if (strstr(code_str, "data_inspection") != NULL ||
                            (msg_str && strstr(msg_str, "inappropriate") != NULL)) {
                        const char *friendly = "当前请求触发了内容安全审核，请换一种说法或稍后再试。";
                        resp->text = strdup(friendly);
                        if (resp->text) {
                            resp->text_len = strlen(resp->text);
                        }
                        cJSON_Delete(root);
                        resp_buf_free(&rb);
                        return ESP_OK;
                    }
                }
                cJSON_Delete(root);
            }
        }
        ESP_LOGE(TAG, "API error %d: %.500s", status, rb.data ? rb.data : "");
        resp_buf_free(&rb);
        return ESP_FAIL;
    }

    // 解析全部的json文件
    cJSON *root = cJSON_Parse(rb.data);

    if (!root) {
        ESP_LOGE(TAG, "Failed to parse API response JSON (status=%d, len=%u, body=%.500s)",
                 status, (unsigned)rb.len, rb.data ? rb.data : "");
        resp_buf_free(&rb);
        return ESP_FAIL;
    }

    resp_buf_free(&rb);

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    cJSON *choice0 = choices && cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
    if (choice0) {
        cJSON *finish = cJSON_GetObjectItem(choice0, "finish_reason");
        if (finish && cJSON_IsString(finish)) {
            resp->tool_use = (strcmp(finish->valuestring, "tool_calls") == 0);
        }

        cJSON *message = cJSON_GetObjectItem(choice0, "message");
        if (message) {
            cJSON *content = cJSON_GetObjectItem(message, "content");
            if (content && cJSON_IsString(content)) {
                size_t tlen = strlen(content->valuestring);
                resp->text = calloc(1, tlen + 1);
                if (resp->text) {
                    memcpy(resp->text, content->valuestring, tlen);
                    resp->text_len = tlen;
                }
            }

            cJSON *tool_calls = cJSON_GetObjectItem(message, "tool_calls");
            if (tool_calls && cJSON_IsArray(tool_calls)) {
                cJSON *tc;
                cJSON_ArrayForEach(tc, tool_calls) {
                    if (resp->call_count >= EC_MAX_TOOL_CALLS) {
                        break;
                    }
                    ec_llm_tool_call_t *call = &resp->calls[resp->call_count];
                    cJSON *id = cJSON_GetObjectItem(tc, "id");
                    cJSON *index = cJSON_GetObjectItem(tc, "index");
                    cJSON *func = cJSON_GetObjectItem(tc, "function");
                    if (id && cJSON_IsString(id)) {
                        strncpy(call->id, id->valuestring, sizeof(call->id) - 1);
                    }
                    if (index && cJSON_IsNumber(index)) {
                        call->index = index->valueint;
                    } else {
                        call->index = resp->call_count;
                    }
                    if (func) {
                        cJSON *name = cJSON_GetObjectItem(func, "name");
                        cJSON *args = cJSON_GetObjectItem(func, "arguments");
                        if (name && cJSON_IsString(name)) {
                            strncpy(call->name, name->valuestring, sizeof(call->name) - 1);
                        }
                        if (args && cJSON_IsString(args)) {
                            call->input = strdup(args->valuestring);
                            if (call->input) {
                                call->input_len = strlen(call->input);
                            }
                        }
                    }
                    resp->call_count++;
                }
                if (resp->call_count > 0) {
                    resp->tool_use = true;
                }
            }
        }
    }

    cJSON_Delete(root);

    ESP_LOGI(TAG, "Response: %d bytes text, %d tool calls, stop=%s",
             (int)resp->text_len, resp->call_count,
             resp->tool_use ? "tool_use" : "end_turn");

    return ESP_OK;
}

static cJSON *convert_messages_openai(const char *system_prompt, cJSON *messages)
{
    cJSON *out = cJSON_CreateArray();
    if (system_prompt && system_prompt[0]) {
        cJSON *sys = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", system_prompt);
        cJSON_AddItemToArray(out, sys);
    }

    if (!messages || !cJSON_IsArray(messages)) {
        return out;
    }

    cJSON *msg;
    cJSON_ArrayForEach(msg, messages) {
        cJSON *role = cJSON_GetObjectItem(msg, "role");
        cJSON *content = cJSON_GetObjectItem(msg, "content");
        if (!role || !cJSON_IsString(role)) {
            continue;
        }

        if (content && cJSON_IsString(content)) {
            cJSON *m = cJSON_CreateObject();
            cJSON_AddStringToObject(m, "role", role->valuestring);
            cJSON_AddStringToObject(m, "content", content->valuestring);
            cJSON_AddItemToArray(out, m);
            continue;
        }

        if (!content || !cJSON_IsArray(content)) {
            continue;
        }

        if (strcmp(role->valuestring, "assistant") == 0) {
            cJSON *m = cJSON_CreateObject();
            cJSON_AddStringToObject(m, "role", "assistant");

            /* collect text */
            char *text_buf = NULL;
            size_t off = 0;
            cJSON *block;
            cJSON *tool_calls = NULL;
            int tool_call_index = 0;
            cJSON_ArrayForEach(block, content) {
                cJSON *btype = cJSON_GetObjectItem(block, "type");
                if (btype && cJSON_IsString(btype) && strcmp(btype->valuestring, "text") == 0) {
                    cJSON *text = cJSON_GetObjectItem(block, "text");
                    if (text && cJSON_IsString(text)) {
                        size_t tlen = strlen(text->valuestring);
                        char *tmp = realloc(text_buf, off + tlen + 1);
                        if (tmp) {
                            text_buf = tmp;
                            memcpy(text_buf + off, text->valuestring, tlen);
                            off += tlen;
                            text_buf[off] = '\0';
                        }
                    }
                } else if (btype && cJSON_IsString(btype) && strcmp(btype->valuestring, "tool_use") == 0) {
                    if (!tool_calls) {
                        tool_calls = cJSON_CreateArray();
                    }
                    cJSON *id = cJSON_GetObjectItem(block, "id");
                    cJSON *name = cJSON_GetObjectItem(block, "name");
                    cJSON *index = cJSON_GetObjectItem(block, "index");
                    cJSON *input = cJSON_GetObjectItem(block, "input");
                    if (!name || !cJSON_IsString(name)) {
                        continue;
                    }

                    cJSON *tc = cJSON_CreateObject();
                    if (id && cJSON_IsString(id)) {
                        cJSON_AddStringToObject(tc, "id", id->valuestring);
                    }
                    if (index && cJSON_IsNumber(index)) {
                        cJSON_AddNumberToObject(tc, "index", index->valueint);
                    } else {
                        cJSON_AddNumberToObject(tc, "index", tool_call_index);
                    }
                    cJSON_AddStringToObject(tc, "type", "function");
                    cJSON *func = cJSON_CreateObject();
                    cJSON_AddStringToObject(func, "name", name->valuestring);
                    if (input) {
                        char *args = cJSON_PrintUnformatted(input);
                        if (args) {
                            cJSON_AddStringToObject(func, "arguments", args);
                            free(args);
                        }
                    }
                    cJSON_AddItemToObject(tc, "function", func);
                    cJSON_AddItemToArray(tool_calls, tc);
                    tool_call_index++;
                }
            }
            if (text_buf) {
                cJSON_AddStringToObject(m, "content", text_buf);
            } else {
                cJSON_AddStringToObject(m, "content", "");
            }
            if (tool_calls) {
                cJSON_AddItemToObject(m, "tool_calls", tool_calls);
            }
            cJSON_AddItemToArray(out, m);
            free(text_buf);
        } else if (strcmp(role->valuestring, "user") == 0) {
            /* tool_result blocks become role=tool */
            cJSON *block;
            bool has_user_text = false;
            char *text_buf = NULL;
            size_t off = 0;
            cJSON_ArrayForEach(block, content) {
                cJSON *btype = cJSON_GetObjectItem(block, "type");
                if (btype && cJSON_IsString(btype) && strcmp(btype->valuestring, "tool_result") == 0) {
                    cJSON *tool_id = cJSON_GetObjectItem(block, "tool_use_id");
                    cJSON *tcontent = cJSON_GetObjectItem(block, "content");
                    if (!tool_id || !cJSON_IsString(tool_id)) {
                        continue;
                    }
                    cJSON *tm = cJSON_CreateObject();
                    cJSON_AddStringToObject(tm, "role", "tool");
                    cJSON_AddStringToObject(tm, "tool_call_id", tool_id->valuestring);
                    if (tcontent && cJSON_IsString(tcontent)) {
                        cJSON_AddStringToObject(tm, "content", tcontent->valuestring);
                    } else {
                        cJSON_AddStringToObject(tm, "content", "");
                    }
                    cJSON_AddItemToArray(out, tm);
                } else if (btype && cJSON_IsString(btype) && strcmp(btype->valuestring, "text") == 0) {
                    cJSON *text = cJSON_GetObjectItem(block, "text");
                    if (text && cJSON_IsString(text)) {
                        size_t tlen = strlen(text->valuestring);
                        char *tmp = realloc(text_buf, off + tlen + 1);
                        if (tmp) {
                            text_buf = tmp;
                            memcpy(text_buf + off, text->valuestring, tlen);
                            off += tlen;
                            text_buf[off] = '\0';
                        }
                        has_user_text = true;
                    }
                }
            }
            if (has_user_text) {
                cJSON *um = cJSON_CreateObject();
                cJSON_AddStringToObject(um, "role", "user");
                cJSON_AddStringToObject(um, "content", text_buf);
                cJSON_AddItemToArray(out, um);
            }
            free(text_buf);
        }
    }

    return out;
}

static esp_err_t resp_buf_append(resp_buf_t *rb, const char *data, size_t len)
{
    while (rb->len + len >= rb->cap) {
        size_t new_cap = rb->cap * 2;
        char *tmp = realloc(rb->data, new_cap);
        if (!tmp) {
            ESP_LOGE(TAG, "Out of memory");
            return ESP_ERR_NO_MEM;
        }
        rb->data = tmp;
        rb->cap = new_cap;
    }
    memcpy(rb->data + rb->len, data, len);
    rb->len += len;
    rb->data[rb->len] = '\0';
    return ESP_OK;
}

static void resp_buf_free(resp_buf_t *rb)
{
    free(rb->data);
    rb->data = NULL;
    rb->len = 0;
    rb->cap = 0;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    resp_buf_t *rb = (resp_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        esp_err_t err = resp_buf_append(rb, (const char *)evt->data, evt->data_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LLM response buffer append failed at %u bytes: %s",
                     (unsigned)(rb ? rb->len : 0), esp_err_to_name(err));
            return err;
        }
    }
    return ESP_OK;
}

static esp_err_t llm_http(const char *post_data, resp_buf_t *rb, int *out_status)
{
    const char *cert_pem = select_server_ca_pem(s_provider.instance.url);
    esp_http_client_config_t config = {
        .url = s_provider.instance.url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = rb,
        .timeout_ms = 120 * 1000,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    if (cert_pem) {
        config.cert_pem = cert_pem;
        config.cert_len = 0;
        ESP_LOGI(TAG, "Using pinned root CA for %s", s_provider.instance.url);
    } else {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");

    char auth[LLM_API_KEY_MAX_LEN + 16];
    snprintf(auth, sizeof(auth), "Bearer %s", s_provider.instance.api_key);
    esp_http_client_set_header(client, "Authorization", auth);

    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    *out_status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);


    return err;
}

static const char *select_server_ca_pem(const char *url)
{
    if (!url) {
        return NULL;
    }

    if (strstr(url, "aliyuncs.com") != NULL) {
        return s_globalsign_root_ca_r3_pem;
    }

    return NULL;
}
