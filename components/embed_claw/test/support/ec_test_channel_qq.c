#include "ec_test_hooks.h"

#define ec_channel_qq ec_channel_qq__test_impl
#include "../../channel/ec_channel_qq.c"
#undef ec_channel_qq

esp_err_t ec_channel_qq_parse_event_for_test(const char *payload_json, ec_msg_t *msg)
{
    cJSON *root;
    cJSON *op_item;
    cJSON *d_item;
    cJSON *t_item;
    const char *event_type;
    esp_err_t err;

    if (!payload_json || !msg) {
        return ESP_ERR_INVALID_ARG;
    }

    root = cJSON_Parse(payload_json);
    if (!root) {
        return ESP_ERR_INVALID_ARG;
    }

    op_item = cJSON_GetObjectItem(root, "op");
    d_item = cJSON_GetObjectItem(root, "d");
    t_item = cJSON_GetObjectItem(root, "t");
    event_type = cJSON_IsString(t_item) ? t_item->valuestring : NULL;

    if (!cJSON_IsNumber(op_item) || op_item->valueint != 0) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }

    err = qq_decode_dispatch_event(event_type, d_item, msg);
    cJSON_Delete(root);
    return err;
}

esp_err_t ec_channel_qq_build_request_for_test(const ec_msg_t *msg,
                                               char *path, size_t path_size,
                                               char *body, size_t body_size)
{
    char path_buf[96];
    char *body_str;

    if (!qq_build_api_path(msg, path_buf, sizeof(path_buf))) {
        return ESP_ERR_INVALID_ARG;
    }

    body_str = qq_build_message_body_alloc(msg);
    if (!body_str) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!path || path_size == 0 || !body || body_size == 0) {
        free(body_str);
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(path, path_size, "%s", path_buf);
    snprintf(body, body_size, "%s", body_str);
    free(body_str);
    return ESP_OK;
}

void ec_channel_qq_reset_state_for_test(void)
{

    s_access_token[0] = '\0';
    s_gateway_url[0] = '\0';
    s_session_id[0] = '\0';
}
