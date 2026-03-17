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

typedef enum {
    LLM_TYPE_OPENAI,
    LLM_TYPE_ANTHROPIC,
    _LLM_TYPE_MAX,
} llm_type_t;

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
    ec_llm_tool_call_t calls[EC_MAX_TOOL_CALLS];
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
 * @brief 通过llm_type初始化LLM模块，设置API Key和模型
 * 
 * @param llm_type LLM 类型，目前支持openai兼容和anthropic兼容两种
 * @return esp_err_t 
 *  - ESP_OK 成功
 *  - ESP_ERR_INVALID_ARG 无效的LLM名称
 */
esp_err_t ec_llm_init(llm_type_t llm_type, const ec_llm_provider_ctx_t* provider_ctx);

esp_err_t ec_llm_chat_tools(const char* system_prompt, cJSON* messages, const char* tools_json, ec_llm_response_t* resp);

esp_err_t ec_llm_response_free(ec_llm_response_t* resp);

/* ==================== [Macros] ============================================ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif  // __EC_LLM_H__
