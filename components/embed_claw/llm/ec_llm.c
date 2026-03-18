/**
 * @file ec_llm.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-05
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "ec_llm.h"
#include "ec_config_internal.h"
#include "ec_llm_openai.h"
#include "esp_log.h"

#include <string.h>

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

/* ==================== [Static Variables] ================================== */

static const char *tag = "ec_llm";

static ec_llm_provider_t *s_providers = NULL;

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_llm_init_default(const ec_llm_provider_ctx_t *provider_ctx)
{
    esp_err_t err;

    if (!provider_ctx) {
        ESP_LOGE(tag, "LLM init failed: provider_ctx must be provided");
        return ESP_ERR_INVALID_ARG;
    }

    if (strcmp(EC_LLM_PROVIDER_NAME, "openai") == 0) {
        s_providers = ec_llm_openai_get_provider();
    }
    // TODO: 添加更多LLM
    else {
        ESP_LOGE(tag, "Unsupported LLM provider '%s' (supported: openai)",
                 EC_LLM_PROVIDER_NAME ? EC_LLM_PROVIDER_NAME : "(null)");
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_providers || !s_providers->vtable || !s_providers->vtable->init ||
            !s_providers->vtable->chat_tools) {
        ESP_LOGE(tag, "LLM provider '%s' is not available",
                 EC_LLM_PROVIDER_NAME ? EC_LLM_PROVIDER_NAME : "(null)");
        s_providers = NULL;
        return ESP_ERR_INVALID_STATE;
    }

    err = s_providers->vtable->init(s_providers, provider_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(tag, "LLM provider '%s' init failed: %s",
                 EC_LLM_PROVIDER_NAME ? EC_LLM_PROVIDER_NAME : "(null)",
                 esp_err_to_name(err));
        s_providers = NULL;
        return err;
    }

    return ESP_OK;
}

esp_err_t ec_llm_chat_tools(const char *system_prompt, cJSON *messages,
                            const char *tools_json, ec_llm_response_t *resp)
{
    if (!s_providers || !s_providers->vtable || !s_providers->vtable->chat_tools) {
        ESP_LOGE(tag, "LLM provider not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!system_prompt || !messages || !resp) {
        ESP_LOGE(tag, "Invalid arguments to ec_llm_chat_tools");
        return ESP_ERR_INVALID_ARG;
    }

    return s_providers->vtable->chat_tools(s_providers, system_prompt, messages, tools_json, resp);
}

esp_err_t ec_llm_response_free(ec_llm_response_t *resp)
{
    if (!resp) {
        return ESP_ERR_INVALID_ARG;
    }

    free(resp->text);
    resp->text = NULL;
    resp->text_len = 0;
    for (int i = 0; i < resp->call_count; i++) {
        free(resp->calls[i].input);
        resp->calls[i].input = NULL;
    }
    resp->call_count = 0;
    resp->tool_use = false;

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */
