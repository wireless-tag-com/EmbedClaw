/**
 * @file main.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-02-28
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_spiffs.h"

#include "nvs_flash.h"

#include "embedclaw_config.h"
#include "wifi_connect.h"

/* ==================== [Defines] =========================================== */

/* ==================== [Typedefs] ========================================== */

/* ==================== [Static Prototypes] ================================= */

static esp_err_t nvs_init(void);
static esp_err_t spiffs_init(void);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "main";

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

void app_main(void)
{
    ESP_LOGI(TAG, "Internal free: %d bytes", (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#if CONFIG_SPIRAM
    ESP_LOGI(TAG, "PSRAM free:    %d bytes", (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#else
    ESP_LOGI(TAG, "PSRAM free:    not available on this target");
#endif

    ESP_ERROR_CHECK(nvs_init());
    ESP_ERROR_CHECK(spiffs_init());
    ESP_ERROR_CHECK(wifi_connect_init());
    wifi_connect_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ==================== [Static Functions] ================================== */

static esp_err_t nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t spiffs_init(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = EMBED_SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ESP_ERROR_CHECK(esp_spiffs_info(NULL, &total, &used));
    ESP_LOGI(TAG, "SPIFFS: total=%d, used=%d", (int)total, (int)used);

    return ESP_OK;
}
