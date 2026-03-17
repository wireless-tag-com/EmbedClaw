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

#include "cJSON.h"
#include "esp_log.h"
#include <ctype.h>
#include <string.h>

/* ==================== [Defines] =========================================== */

#define EC_CHANNEL_NAME
#include "channel/ec_channel_reg.inc"

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

#define EC_CHANNEL_REG_EXTERN
#include "channel/ec_channel_reg.inc"

static bool channel_names_equal(const char *lhs, const char *rhs);
static const ec_channel_t *find_channel(const char *channel);

/* ==================== [Static Variables] ================================== */

const char g_ec_channel_system[] = "system";

static const char *TAG = "channel";

/*
 * Keep a few spare slots beyond the built-in channels so tests or future
 * custom drivers can register extra providers without colliding with the
 * compile-time enum count.
 */

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
    if (!driver || !driver->name || driver->name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

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

esp_err_t ec_channel_start(void)
{
    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (s_channel[i]) {
            esp_err_t err = s_channel[i]->vtable.start();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Starting channel: %s", s_channel[i]->name);
            } else {
                ESP_LOGE(TAG, "Starting channel failed: %s", s_channel[i]->name);
            }
        }
    }

    return ESP_OK;
}

esp_err_t ec_channel_send(const ec_msg_t *msg)
{
    const ec_channel_t *driver;

    if (!msg || msg->channel[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    driver = find_channel(msg->channel);
    if (!driver) {
        ESP_LOGW(TAG, "Unknown channel: %s", msg->channel);
        return ESP_ERR_NOT_FOUND;
    }

    if (!driver->vtable.send) {
        ESP_LOGW(TAG, "Channel has no send handler: %s", msg->channel);
        return ESP_ERR_NOT_SUPPORTED;
    }

    return driver->vtable.send(msg);
}


/* ==================== [Static Functions] ================================== */

static bool channel_names_equal(const char *lhs, const char *rhs)
{
    if (!lhs || !rhs) {
        return false;
    }

    return (lhs == rhs) || (strcmp(lhs, rhs) == 0);
}

static const ec_channel_t *find_channel(const char *channel)
{
    if (!channel || channel[0] == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < _EC_CHANNEL_ENMU_MAX; i++) {
        if (s_channel[i] && channel_names_equal(s_channel[i]->name, channel)) {
            return s_channel[i];
        }
    }

    return NULL;
}
