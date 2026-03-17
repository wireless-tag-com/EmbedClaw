#ifndef __EC_TEST_HOOKS_H__
#define __EC_TEST_HOOKS_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "esp_err.h"
#include "cJSON.h"

#include "core/ec_agent.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ec_test_spiffs_mount(void);
void ec_test_spiffs_unmount(void);

bool ec_tools_get_time_format_epoch_for_test(time_t epoch, char *out, size_t out_size);
void ec_tools_web_search_format_results_for_test(const char *response_json, char *output, size_t output_size);
bool ec_tools_files_validate_path_for_test(const char *path);
esp_err_t ec_tools_files_replace_first_for_test(const char *source, const char *old_str,
                                                const char *new_str, char *output, size_t output_size);

esp_err_t ec_channel_ws_parse_payload_for_test(int fd, const char *payload_json, ec_msg_t *msg);
void ec_channel_ws_add_client_for_test(int fd, const char *chat_id);
void ec_channel_ws_reset_state_for_test(void);
esp_err_t ec_channel_ws_build_response_json_for_test(const ec_msg_t *msg, char *output, size_t output_size);

esp_err_t ec_channel_qq_parse_event_for_test(const char *payload_json, ec_msg_t *msg);
esp_err_t ec_channel_qq_build_request_for_test(const ec_msg_t *msg,
                                               char *path, size_t path_size,
                                               char *body, size_t body_size);
void ec_channel_qq_reset_state_for_test(void);

bool ec_channel_feishu_parse_frame_for_test(const uint8_t *buf, size_t len,
                                            int32_t *method, int32_t *service,
                                            uint64_t *seq_id, uint64_t *log_id,
                                            char *type_val, size_t type_len,
                                            char *payload, size_t payload_size);
int ec_channel_feishu_encode_ping_for_test(uint8_t *out, size_t out_max, int32_t service_id);
int ec_channel_feishu_encode_response_for_test(uint8_t *out, size_t out_max,
                                               uint64_t seq_id, uint64_t log_id, int32_t service);

cJSON *ec_llm_openai_convert_tools_for_test(const char *tools_json);
cJSON *ec_llm_openai_convert_messages_for_test(const char *system_prompt, cJSON *messages);
const char *ec_llm_openai_select_server_ca_pem_for_test(const char *url);

#ifdef __cplusplus
}
#endif

#endif // __EC_TEST_HOOKS_H__
