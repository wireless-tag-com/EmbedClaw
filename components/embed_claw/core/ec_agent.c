/**
 * @file ec_agent.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "ec_agent.h"
#include "ec_channel.h"
#include "ec_config_internal.h"
#include "llm/ec_llm.h"

#include "ec_memory.h"
#include "ec_session.h"
#include "ec_tools.h"
#include "ec_skill_loader.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* ==================== [Defines] =========================================== */

#define EC_AGENT_TOOL_RESULT_MAX_FOR_LLM 4096

#define EC_AGENT_TOOL_OUTPUT_SIZE  (4 * 1024)
#define EC_AGENT_PROMPT_SCRATCH_SIZE 4096
#define EC_AGENT_SESSION_KEY_MAX (sizeof(((ec_msg_t *)0)->channel) + \
                            sizeof(((ec_msg_t *)0)->chat_type) + \
                            sizeof(((ec_msg_t *)0)->chat_id) + 3)

#define EC_AGENT_CHANNEL_DELIVERY_TASK_STACK (8 * 1024)
#define EC_AGENT_CHANNEL_DELIVERY_TASK_PRIO  5

#define EC_AGENT_SYSTEM_PROMPT_HEAD \
        "You are EmbedClaw, a helpful and concise AI assistant running on an ESP32 device.\n"\
        "You communicate via Feishu and WebSocket.\n"\
        "Reply briefly to short messages (e.g. 你好, 在吗, 谢谢).\n"\
        "# Tools\n"\
        "Available tools:\n"

#define EC_AGENT_SYSTEM_PROMPT_TAIL \
        "\nWhen using cron_add to reply later in the same conversation, reuse the current channel, chat_type, and chat_id.\n\n"\
        "Use tools when needed. Provide your final answer as text after using tools.\n\n"\
        "## Memory\n"\
        "You have persistent memory stored on local flash:\n"\
        "- Long-term memory: " EC_FS_MEMORY_DIR "/MEMORY.md\n"\
        "- Daily notes: " EC_FS_MEMORY_DIR "/daily/<YYYY-MM-DD>.md\n\n"\
        "IMPORTANT: Actively use memory to remember things across conversations.\n"\
        "- When you learn something new about the user (name, preferences, habits, context), write it to MEMORY.md.\n"\
        "- When something noteworthy happens in a conversation, append it to today's daily note.\n"\
        "- Always read_file MEMORY.md before writing, so you can edit_file to update without losing existing content.\n"\
        "- Use get_current_time to know today's date before writing daily notes.\n"\
        "- Keep MEMORY.md concise and organized — summarize, don't dump raw conversation.\n"\
        "- You should proactively save memory without being asked. If the user tells you their name, preferences, or important facts, persist them immediately.\n\n"\
        "## Skills\n"\
        "Skills are specialized instruction files stored in " EC_SKILLS_PREFIX ".\n"\
        "When a task matches a skill, read the full skill file for detailed instructions.\n"\
        "You can create new skills using write_file to " EC_SKILLS_PREFIX "<name>.md.\n"

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static void agent_loop_task(void *arg);
static void channel_delivery_task(void *arg);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "agent";
static QueueHandle_t s_inbound_queue;
static QueueHandle_t s_outbound_queue;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_agent_start(void)
{
    s_inbound_queue = xQueueCreate(EC_AGENT_BUS_QUEUE_LEN, sizeof(ec_msg_t));
    s_outbound_queue = xQueueCreate(EC_AGENT_BUS_QUEUE_LEN, sizeof(ec_msg_t));

    if (!s_inbound_queue || !s_outbound_queue) {
        ESP_LOGE(TAG, "Failed to create message queues");
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret = xTaskCreatePinnedToCore(
                         agent_loop_task, "agent_loop",
                         EC_AGENT_STACK, NULL,
                         EC_AGENT_PRIO, NULL, EC_AGENT_CORE);

    if (ret != pdPASS) {
        return ESP_FAIL;
    }

    ret = xTaskCreate(channel_delivery_task, "channel_delivery",
                      EC_AGENT_CHANNEL_DELIVERY_TASK_STACK, NULL,
                      EC_AGENT_CHANNEL_DELIVERY_TASK_PRIO, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "channel delivery task create failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ec_agent_inbound(const ec_msg_t *msg)
{
    if (xQueueSend(s_inbound_queue, msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Inbound queue full, dropping message");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static cJSON *build_assistant_content(const ec_llm_response_t *resp)
{
    cJSON *content = cJSON_CreateArray();

    /* Text block */
    if (resp->text && resp->text_len > 0) {
        cJSON *text_block = cJSON_CreateObject();
        cJSON_AddStringToObject(text_block, "type", "text");
        cJSON_AddStringToObject(text_block, "text", resp->text);
        cJSON_AddItemToArray(content, text_block);
    }

    /* Tool use blocks */
    for (int i = 0; i < resp->call_count; i++) {
        const ec_llm_tool_call_t *call = &resp->calls[i];
        cJSON *tool_block = cJSON_CreateObject();
        cJSON_AddStringToObject(tool_block, "type", "tool_use");
        cJSON_AddStringToObject(tool_block, "id", call->id);
        cJSON_AddStringToObject(tool_block, "name", call->name);
        cJSON_AddNumberToObject(tool_block, "index", call->index);

        cJSON *input = cJSON_Parse(call->input);
        if (input) {
            cJSON_AddItemToObject(tool_block, "input", input);
        } else {
            cJSON_AddItemToObject(tool_block, "input", cJSON_CreateObject());
        }

        cJSON_AddItemToArray(content, tool_block);
    }

    return content;
}

static void json_set_string(cJSON *obj, const char *key, const char *value)
{
    if (!obj || !key || !value) {
        return;
    }
    cJSON_DeleteItemFromObject(obj, key);
    cJSON_AddStringToObject(obj, key, value);
}

static void build_session_key(const ec_msg_t *msg, char *buf, size_t size)
{
    if (!buf || size == 0) {
        return;
    }

    if (!msg) {
        buf[0] = '\0';
        return;
    }

    snprintf(buf, size, "%s|%s|%s",
             msg->channel[0] ? msg->channel : "",
             msg->chat_type[0] ? msg->chat_type : "",
             msg->chat_id[0] ? msg->chat_id : "");
}

static char *patch_tool_input_with_context(const ec_llm_tool_call_t *call, const ec_msg_t *msg)
{
    if (!call || !msg || strcmp(call->name, "cron_add") != 0) {
        return NULL;
    }

    cJSON *root = cJSON_Parse(call->input ? call->input : "{}");
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        root = cJSON_CreateObject();
    }
    if (!root) {
        return NULL;
    }

    bool changed = false;

    cJSON *channel_item = cJSON_GetObjectItem(root, "channel");
    const char *channel = cJSON_IsString(channel_item) ? channel_item->valuestring : NULL;

    if ((!channel || channel[0] == '\0') && msg->channel[0] != '\0') {
        json_set_string(root, "channel", msg->channel);
        channel = msg->channel;
        changed = true;
    }

    if (channel && strcmp(channel, g_ec_channel_system) != 0 &&
            strcmp(channel, msg->channel) == 0) {
        cJSON *chat_item = cJSON_GetObjectItem(root, "chat_id");
        const char *chat_id = cJSON_IsString(chat_item) ? chat_item->valuestring : NULL;
        if (msg->chat_id[0] != '\0' &&
                (!chat_id || chat_id[0] == '\0' || strcmp(chat_id, "cron") == 0)) {
            json_set_string(root, "chat_id", msg->chat_id);
            changed = true;
        }

        chat_item = cJSON_GetObjectItem(root, "chat_type");
        const char *chat_type = cJSON_IsString(chat_item) ? chat_item->valuestring : NULL;
        if (msg->chat_type[0] != '\0' &&
                (!chat_type || chat_type[0] == '\0' || strcmp(chat_type, "cron") == 0)) {
            json_set_string(root, "chat_type", msg->chat_type);
            changed = true;
        }
    }

    char *patched = NULL;
    if (changed) {
        patched = cJSON_PrintUnformatted(root);
        if (patched) {
            ESP_LOGI(TAG, "Patched cron_add target to %s:%s:%s", msg->channel, msg->chat_type, msg->chat_id);
            ESP_LOGI(TAG, "cron_add patched input: %.200s", patched);
        }
    }

    cJSON_Delete(root);
    return patched;
}

static cJSON *build_tool_results(const ec_llm_response_t *resp, const ec_msg_t *msg,
                                 char *tool_output, size_t tool_output_size)
{
    cJSON *content = cJSON_CreateArray();

    for (int i = 0; i < resp->call_count; i++) {
        const ec_llm_tool_call_t *call = &resp->calls[i];
        const char *tool_input = call->input ? call->input : "{}";
        char *patched_input = patch_tool_input_with_context(call, msg);
        if (patched_input) {
            tool_input = patched_input;
        }

        if (strcmp(call->name, "cron_add") == 0) {
            ESP_LOGI(TAG, "Tool cron_add input: %.200s", tool_input ? tool_input : "(null)");
        }

        /* Execute tool */
        tool_output[0] = '\0';
        ec_tools_execute(call->name, tool_input, tool_output, tool_output_size);
        free(patched_input);

        ESP_LOGI(TAG, "Tool %s result: %d bytes", call->name, (int)strlen(tool_output));
        if (strcmp(call->name, "cron_add") == 0) {
            ESP_LOGI(TAG, "Tool cron_add output: %.200s", tool_output);
        }

        /* Build tool_result block */
        cJSON *result_block = cJSON_CreateObject();
        cJSON_AddStringToObject(result_block, "type", "tool_result");
        cJSON_AddStringToObject(result_block, "tool_use_id", call->id);
        cJSON_AddStringToObject(result_block, "content", tool_output);
        cJSON_AddItemToArray(content, result_block);
    }

    return content;
}

static size_t append_file(char *buf, size_t size, size_t offset, const char *path, const char *header)
{
    if (!buf || size == 0 || offset >= size) {
        return size ? (size - 1) : 0;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        return offset;
    }

    if (header && offset < size - 1) {
        offset += snprintf(buf + offset, size - offset, "\n## %s\n\n", header);
        if (offset >= size) {
            offset = size - 1;
        }
    }

    if (offset >= size - 1) {
        fclose(f);
        return size - 1;
    }

    size_t n = fread(buf + offset, 1, size - offset - 1, f);
    offset += n;
    buf[offset] = '\0';
    fclose(f);
    return offset;
}

static esp_err_t context_build_system_prompt(char *buf, size_t size)
{
    char *scratch = NULL;

    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t off = 0;
    size_t cap = size - 1;

    off += snprintf(buf + off, size - off, EC_AGENT_SYSTEM_PROMPT_HEAD);
    if (off > cap) {
        off = cap;
    }

    scratch = calloc(1, EC_AGENT_PROMPT_SCRATCH_SIZE);
    if (!scratch) {
        ESP_LOGW(TAG, "Skipping optional prompt sections: out of memory");
    } else {
        size_t tools_len = ec_tools_build_summary(scratch, EC_AGENT_PROMPT_SCRATCH_SIZE);
        if (off < cap && tools_len > 0) {
            off += snprintf(buf + off, size - off, "%s", scratch);
            if (off > cap) {
                off = cap;
            }
        }
    }

    off += snprintf(buf + off, size - off, EC_AGENT_SYSTEM_PROMPT_TAIL);
    if (off > cap) {
        off = cap;
    }

    /* Bootstrap files */
    off = append_file(buf, size, off, EC_SOUL_FILE, "Personality");
    off = append_file(buf, size, off, EC_USER_FILE, "User Info");

    if (scratch) {
        if (off < cap && ec_memory_read_long_term(scratch, EC_AGENT_PROMPT_SCRATCH_SIZE) == ESP_OK && scratch[0]) {
            off += snprintf(buf + off, size - off, "\n## Long-term Memory\n\n%s\n", scratch);
            if (off > cap) {
                off = cap;
            }
        }

        scratch[0] = '\0';
        if (off < cap && ec_memory_read_recent(scratch, EC_AGENT_PROMPT_SCRATCH_SIZE, 3) == ESP_OK && scratch[0]) {
            off += snprintf(buf + off, size - off, "\n## Recent Notes\n\n%s\n", scratch);
            if (off > cap) {
                off = cap;
            }
        }

        scratch[0] = '\0';
        size_t skills_len = ec_skill_loader_build_summary(scratch, EC_AGENT_PROMPT_SCRATCH_SIZE);
        if (off < cap && skills_len > 0) {
            off += snprintf(buf + off, size - off,
                            "\n## Available Skills\n\n"
                            "Available skills (use read_file to load full instructions):\n%s\n",
                            scratch);
            if (off > cap) {
                off = cap;
            }
        }

        free(scratch);
    }

    if (off >= size) {
        buf[size - 1] = '\0';
        off = size - 1;
    }

    ESP_LOGI(TAG, "System prompt built: %d bytes", (int)off);

    return ESP_OK;
}

static void append_turn_context_prompt(char *prompt, size_t size, const ec_msg_t *msg)
{
    if (!prompt || size == 0 || !msg) {
        return;
    }

    size_t off = strnlen(prompt, size - 1);
    if (off >= size - 1) {
        return;
    }

    int n = snprintf(
                prompt + off, size - off,
                "\n## Current Turn Context\n"
                "- source_channel: %s\n"
                "- source_chat_id: %s\n"
                "- source_chat_type: %s\n"
                "- If using cron_add to reply back in this conversation, reuse source_channel/source_chat_type/source_chat_id.\n"
                "- Never leave chat_type or chat_id as 'cron' for non-system channels.\n",
                msg->channel[0] ? msg->channel : "(unknown)",
                msg->chat_id[0] ? msg->chat_id : "(empty)",
                msg->chat_type[0] ? msg->chat_type : "(empty)");

    if (n < 0 || (size_t)n >= (size - off)) {
        prompt[size - 1] = '\0';
    }
}

static void agent_loop_task(void *arg)
{
    esp_err_t err = ESP_OK;

    char *final_text = NULL; // 存储最终文本回复，用于发送给用户
    char *system_prompt = (char *)malloc(EC_AGENT_CONTEXT_BUF_SIZE + EC_LLM_STREAM_BUF_SIZE + EC_AGENT_TOOL_OUTPUT_SIZE);
    if (!system_prompt) {
        ESP_LOGE(TAG, "Failed to allocate agent buffers");
        vTaskDelete(NULL);
        return;
    }

    char *history_json = system_prompt + EC_AGENT_CONTEXT_BUF_SIZE;
    char *tool_output = history_json + EC_LLM_STREAM_BUF_SIZE;

    // 获取所有工具的 JSON 描述，供后续 LLM 调用时使用
    const char *tools_json = ec_tools_get_json();

    while (1) {
        // 获取入站消息，统一进行处理
        // 该消息来源于ws、飞书等适配器，或者系统适配器（定时任务触发等）
        ec_msg_t msg = {0};
        char session_key[EC_AGENT_SESSION_KEY_MAX];
        volatile bool sent_working_status = false;

        if (xQueueReceive(s_inbound_queue, &msg, UINT32_MAX) != pdTRUE) {
            continue;
        }

        ESP_LOGI(TAG, "Processing message from %s:%s:%s", msg.channel, msg.chat_id, msg.chat_type);
        build_session_key(&msg, session_key, sizeof(session_key));

        // 构建系统提示词，包含基本信息和当前消息的上下文（来源渠道、chat_id等）
        context_build_system_prompt(system_prompt, EC_AGENT_CONTEXT_BUF_SIZE);
        append_turn_context_prompt(system_prompt, EC_AGENT_CONTEXT_BUF_SIZE, &msg);
        ESP_LOGI(TAG, "LLM turn context: channel=%s chat_type=%s chat_id=%s",
                 msg.channel, msg.chat_type, msg.chat_id);

        // 读取当前会话历史，构建消息数组供 LLM 使用
        ec_session_get_history_json(session_key, history_json,
                                    EC_LLM_STREAM_BUF_SIZE, EC_AGENT_MAX_HISTORY);
        cJSON *messages = cJSON_Parse(history_json);
        if (!messages) {
            messages = cJSON_CreateArray();
        }
        int history_count = cJSON_GetArraySize(messages);

        cJSON *user_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", msg.content);
        cJSON_AddItemToArray(messages, user_msg);

        // 进入 ReAct 循环，最多迭代 EC_AGENT_MAX_TOOL_ITER 次
        for (size_t i = 0; i < EC_AGENT_MAX_TOOL_ITER; i++) {
#if EC_AGENT_SEND_WORKING_STATUS
            if (!sent_working_status && strcmp(msg.channel, g_ec_channel_system) != 0) {
                ec_msg_t status = {0};
                strncpy(status.channel, msg.channel, sizeof(status.channel) - 1);
                strncpy(status.chat_id, msg.chat_id, sizeof(status.chat_id) - 1);
                strncpy(status.chat_type, msg.chat_type, sizeof(status.chat_type) - 1);
                status.content = strdup("\xF0\x9F\x90\xB1" "agent is working...");
                if (status.content) {
                    if (xQueueSend(s_outbound_queue, &status, pdMS_TO_TICKS(1000)) != pdTRUE) {
                        ESP_LOGW(TAG, "Outbound queue full, drop working status");
                        free(status.content);
                    } else {
                        sent_working_status = true;
                    }
                }
            }
#endif
            ec_llm_response_t resp;
            err = ec_llm_chat_tools(system_prompt, messages, tools_json, &resp);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "LLM call failed: %s", esp_err_to_name(err));
                break;
            }

            if (!resp.tool_use) {
                if (resp.text && resp.text_len > 0) {
                    final_text = strdup(resp.text);
                }
                ec_llm_response_free(&resp);
                break;
            }

            cJSON *asst_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(asst_msg, "role", "assistant");
            cJSON_AddItemToObject(asst_msg, "content", build_assistant_content(&resp));
            cJSON_AddItemToArray(messages, asst_msg);

            cJSON *tool_results = build_tool_results(&resp, &msg, tool_output, EC_AGENT_TOOL_OUTPUT_SIZE);
            cJSON *result_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(result_msg, "role", "user");
            cJSON_AddItemToObject(result_msg, "content", tool_results);
            cJSON_AddItemToArray(messages, result_msg);

            ec_llm_response_free(&resp);
        }

        if (final_text && final_text[0]) {
            int total = cJSON_GetArraySize(messages);
            bool save_ok = true;
            for (int k = history_count; k < total; k++) {
                if (ec_session_append_msg(session_key, cJSON_GetArrayItem(messages, k)) != ESP_OK) {
                    save_ok = false;
                }
            }
            if (ec_session_append(session_key, "assistant", final_text) != ESP_OK) {
                save_ok = false;
            }
            if (!save_ok) {
                ESP_LOGW(TAG, "Session save failed for %s:%s:%s",
                         msg.channel, msg.chat_type, msg.chat_id);
            } else {
                ESP_LOGI(TAG, "Session saved for %s:%s:%s", msg.channel, msg.chat_type, msg.chat_id);
            }

            // 推送消息到出站队列，发送给用户
            ec_msg_t out = {0};
            strncpy(out.channel, msg.channel, sizeof(out.channel) - 1);
            strncpy(out.chat_id, msg.chat_id, sizeof(out.chat_id) - 1);
            strncpy(out.chat_type, msg.chat_type, sizeof(out.chat_type) - 1);
            out.content = final_text;  /* transfer ownership */
            ESP_LOGI(TAG, "Queue final response to %s:%s (%d bytes)",
                     out.channel, out.chat_id, (int)strlen(final_text));
            if (xQueueSend(s_outbound_queue, &out, pdMS_TO_TICKS(1000)) != pdTRUE) {
                ESP_LOGW(TAG, "Outbound queue full, drop final response");
                free(final_text);
            } else {
                final_text = NULL;
            }
        } else {
            /* Error or empty response */
            free(final_text);
            ec_msg_t out = {0};
            strncpy(out.channel, msg.channel, sizeof(out.channel) - 1);
            strncpy(out.chat_id, msg.chat_id, sizeof(out.chat_id) - 1);
            strncpy(out.chat_type, msg.chat_type, sizeof(out.chat_type) - 1);
            out.content = strdup("Sorry, I encountered an error.");
            if (out.content) {
                if (xQueueSend(s_outbound_queue, &out, pdMS_TO_TICKS(1000)) != pdTRUE) {
                    ESP_LOGW(TAG, "Outbound queue full, drop error response");
                    free(out.content);
                }
            }
        }
        free(msg.content);
#if CONFIG_SPIRAM
        ESP_LOGI(TAG, "Free PSRAM: %d bytes", (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#else
        ESP_LOGI(TAG, "Free internal heap: %d bytes", (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#endif
    }

}

static void channel_delivery_task(void *arg)
{
    (void)arg;

    while (1) {
        ec_msg_t msg = {0};
        esp_err_t err;

        if (xQueueReceive(s_outbound_queue, &msg, UINT32_MAX) != pdTRUE) {
            continue;
        }

        ESP_LOGI(TAG, "Deliver outbound message to %s:%s:%s (%d bytes)",
                 msg.channel, msg.chat_type, msg.chat_id, msg.content ? (int)strlen(msg.content) : 0);
        err = ec_channel_send(&msg);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Send outbound to:%s failed: %s",
                     msg.chat_id, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Outbound delivered to %s:%s:%s", msg.channel, msg.chat_type, msg.chat_id);
        }

        free(msg.content);
    }
}
