#include "ec_test_hooks.h"

#define ec_channel_ws ec_channel_ws__test_impl
#include "../../channel/ec_channel_ws.c"
#undef ec_channel_ws

void ec_channel_ws_add_client_for_test(int fd, const char *chat_id)
{
    ws_client_t *client = add_client(fd);
    if (client && chat_id && chat_id[0]) {
        strncpy(client->chat_id, chat_id, sizeof(client->chat_id) - 1);
    }
}

void ec_channel_ws_reset_state_for_test(void)
{
    memset(s_clients, 0, sizeof(s_clients));
    s_server = NULL;
}

esp_err_t ec_channel_ws_parse_payload_for_test(int fd, const char *payload_json, ec_msg_t *msg)
{
    return ws_decode_message(fd, payload_json, msg);
}

esp_err_t ec_channel_ws_build_response_json_for_test(const ec_msg_t *msg, char *output, size_t output_size)
{
    char *json_str = ws_build_response_json_alloc(msg);
    if (!json_str) {
        return ESP_ERR_NO_MEM;
    }

    if (!output || output_size == 0) {
        free(json_str);
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(output, output_size, "%s", json_str);
    free(json_str);
    return ESP_OK;
}
