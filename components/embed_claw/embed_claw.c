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

#define EC_OUTBOUND_TASK_STACK 4096
#define EC_OUTBOUND_TASK_PRIO  5

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static void outbound_dispatch_task(void *arg);

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
    ESP_ERROR_CHECK(ec_llm_init(LLM_TYPE_OPENAI, &s_llm_provider_ctx));
    ESP_ERROR_CHECK(ec_agent_start());
    ESP_ERROR_CHECK(ec_channel_start());

    BaseType_t task_ok = xTaskCreate(
                             outbound_dispatch_task,
                             "ec_outbound",
                             EC_OUTBOUND_TASK_STACK,
                             NULL,
                             EC_OUTBOUND_TASK_PRIO,
                             NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "outbound task create failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "EmbedClaw started");

    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static void outbound_dispatch_task(void *arg)
{
    (void)arg;

    while (1) {
        ec_msg_t msg = {0};
        esp_err_t err = ec_agent_outbound(&msg, UINT32_MAX);
        if (err != ESP_OK) {
            continue;
        }

        ESP_LOGI(TAG, "Dispatch outbound to %s:%s (%d bytes)",
                 msg.channel, msg.chat_id, msg.content ? (int)strlen(msg.content) : 0);
        err = ec_channel_send(&msg);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Send outbound to:%s failed: %s",
                     msg.chat_id, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Outbound delivered to %s:%s", msg.channel, msg.chat_id);
        }

        free(msg.content);
    }
}
