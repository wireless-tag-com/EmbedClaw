/**
 * @file ec_llm.h
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#ifndef __EC_LLM_H__
#define __EC_LLM_H__

/* ==================== [Includes] ========================================== */

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "cJSON.h"

#include "ec_config_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

typedef struct {
    char id[64];
    char name[64];
    int index;
    char *input;
    size_t input_len;
} ec_llm_tool_call_t;

typedef struct {
    char *text;
    size_t text_len;
    ec_llm_tool_call_t calls[EC_LLM_MAX_TOOL_CALLS];
    int call_count;
    bool tool_use;
} ec_llm_response_t;

typedef struct _ec_llm_provider_ctx_t{
    const char* url;
    const char* api_key;
    const char* model;
} ec_llm_provider_ctx_t;

/* ==================== [Global Prototypes] ================================= */

/**
 * @brief 使用 EC_LLM_PROVIDER_NAME 配置初始化 LLM 模块
 *
 * @param provider_ctx provider 上下文（url/api_key/model）
 * @return esp_err_t
 *  - ESP_OK 成功
 *  - ESP_ERR_INVALID_ARG 参数非法或 provider 不支持
 */
esp_err_t ec_llm_init_default(const ec_llm_provider_ctx_t *provider_ctx);

esp_err_t ec_llm_chat_tools(const char* system_prompt, cJSON* messages, const char* tools_json, ec_llm_response_t* resp);

esp_err_t ec_llm_response_free(ec_llm_response_t* resp);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // __EC_LLM_H__
