/**
 * @file embed_claw.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-06
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "embed_claw.h"

#include "core/ec_agent.h"
#include "core/ec_channel.h"
#include "core/ec_skill_loader.h"
#include "core/ec_tools.h"
#include "llm/ec_llm.h"
#include "ec_config_internal.h"

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_log.h"
/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Variables] ================================== */

static const ec_llm_provider_ctx_t s_llm_provider_ctx = {
    .api_key = EC_LLM_API_KEY,
    .model = EC_LLM_MODEL,
    .url = EC_LLM_API_URL,
};

static const char *TAG = "embed_claw";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_embed_claw_start(void)
{
    ESP_ERROR_CHECK(ec_channel_register_all());
    ESP_ERROR_CHECK(ec_skill_loader_init());
    ESP_ERROR_CHECK(ec_tools_register_all());
    ESP_ERROR_CHECK(ec_llm_init_default(&s_llm_provider_ctx));
    ESP_ERROR_CHECK(ec_agent_start());
    ESP_ERROR_CHECK(ec_channel_start());

    ESP_LOGI(TAG, "EmbedClaw started");

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */
