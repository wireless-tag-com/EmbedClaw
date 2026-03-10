/**
 * @file wifi_connect.cpp
 * @author cangyu (sky.kirto@qq.com)
 * @brief WiFi connection bridge (C interface over C++ wifi manager)
 * @version 0.1
 * @date 2026-03-02
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

#include "wifi_connect.h"

#include <string>

#include "esp_log.h"

#include "wifi_manager.h"
#include "ssid_manager.h"

#include "embed_claw.h"
#include "esp_system.h"

#include "embedclaw_config.h"

static const char *TAG = "wifi_connect";

static void wifi_event_handler(WifiEvent event, const std::string &data)
{
    switch (event) {
        case WifiEvent::Scanning:
            ESP_LOGI(TAG, "Scanning for networks...");
            break;
        case WifiEvent::Connecting:
            ESP_LOGI(TAG, "Connecting to network...");
            break;
        case WifiEvent::Connected:
            ESP_LOGI(TAG, "Connected successfully!");
            ESP_ERROR_CHECK(ec_embed_claw_start());
            break;
        case WifiEvent::Disconnected:
            ESP_LOGW(TAG, "Disconnected from network, reason: %s", data.c_str());
            break;
        case WifiEvent::ConfigModeEnter:
            ESP_LOGI(TAG, "Entered config mode");
            break;
        case WifiEvent::ConfigModeExit:
            ESP_LOGI(TAG, "Exited config mode");
            esp_restart();
            break;
    }
}

extern "C" esp_err_t wifi_connect_init(void)
{
    auto &wifi_manager = WifiManager::GetInstance();

    WifiManagerConfig config;
    config.ssid_prefix = EMBED_WIFI_SSID_PREFIX;
    config.language = EMBED_WIFI_LANGUAGE;

    if (!wifi_manager.Initialize(config)) {
        ESP_LOGE(TAG, "WifiManager initialization failed");
        return ESP_FAIL;
    }

    wifi_manager.SetEventCallback(wifi_event_handler);
    return ESP_OK;
}

extern "C" void wifi_connect_start(void)
{
    auto &wifi_manager = WifiManager::GetInstance();
    auto &ssid_list = SsidManager::GetInstance().GetSsidList();

    if (ssid_list.empty()) {
        wifi_manager.StartConfigAp();
        return;
    }

    wifi_manager.StartStation();
}
