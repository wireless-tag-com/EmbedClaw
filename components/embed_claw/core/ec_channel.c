/**
 * @file ec_channel.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-06
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "ec_channel.h"

#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

#define EC_CHANNEL_REG_EXTERN
#include "channel/ec_channel_reg.inc"

/* ==================== [Static Variables] ================================== */

static const char *TAG = "channel";

static const ec_channel_t *s_channel[_EC_CHANNEL_ENMU_MAX] = {0};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_channel_register_all(void)
{
#define EC_CHANNEL_REG_FUNC
#include "channel/ec_channel_reg.inc"

    return ESP_OK;
}

esp_err_t ec_channel_register(const ec_channel_t *driver)
{
    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (!s_channel[i]) {
            s_channel[i] = driver;
            ESP_LOGI(TAG, "Registered channel: %s", driver->name);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Channel registry full, cannot register: %s", driver->name);
    return ESP_ERR_NO_MEM;
}

esp_err_t ec_channel_start(const char *channel)
{

    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (!s_channel[i]) {
            continue;
        }

        if (s_channel[i]->name == channel) {
            ESP_LOGI(TAG, "Starting channel: %s", channel);
            return s_channel[i]->vtable.start();
        }
    }

    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (!s_channel[i]) {
            continue;
        }

        if (strcmp(s_channel[i]->name, channel) == 0) {
            ESP_LOGI(TAG, "Starting channel: %s", channel);
            return s_channel[i]->vtable.start();
        }
    }

    ESP_LOGW(TAG, "Unknown channel: %s", channel);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ec_channel_send(const ec_msg_t *msg)
{


    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (!s_channel[i]) {
            continue;
        }
        if (s_channel[i]->name == msg->channel) {
            return s_channel[i]->vtable.send(msg);
        }
    }

    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (!s_channel[i]) {
            continue;
        }
        if (strcmp(s_channel[i]->name, msg->channel) == 0) {
            return s_channel[i]->vtable.send(msg);
        }
    }

    ESP_LOGW(TAG, "Unknown channel: %s", msg->channel);
    return ESP_ERR_NOT_FOUND;
}

/* ==================== [Static Functions] ================================== */
