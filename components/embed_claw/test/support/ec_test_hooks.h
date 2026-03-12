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
void ec_tools_cron_configure_for_test(bool skip_task_start, bool skip_persist);

esp_err_t ec_channel_ws_parse_payload_for_test(int fd, const char *payload_json, ec_msg_t *msg);
void ec_channel_ws_add_client_for_test(int fd, const char *chat_id);
void ec_channel_ws_reset_state_for_test(void);
esp_err_t ec_channel_ws_build_response_json_for_test(const ec_msg_t *msg, char *output, size_t output_size);

void ec_channel_feishu_parse_chat_id_for_test(const char *chat_id, char *out_type, size_t type_len,
                                              char *out_id, size_t id_len);
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
char *ec_agent_patch_tool_input_with_context_for_test(const char *tool_name, const char *input_json,
                                                      const ec_msg_t *msg);
esp_err_t ec_agent_build_system_prompt_for_test(char *buf, size_t size);
void ec_agent_append_turn_context_for_test(char *prompt, size_t size, const ec_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif // __EC_TEST_HOOKS_H__
