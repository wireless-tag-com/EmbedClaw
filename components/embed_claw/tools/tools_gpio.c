/**
 * @file tools_gpio.c
 * @author cangyu (sky.kirto@qq.com)
 * @brief
 * @version 0.1
 * @date 2026-03-26
 *
 * @copyright Copyright (c) 2026, Wireless-Tag. All rights reserved.
 *
 */

/* ==================== [Includes] ========================================== */

#include "core/ec_tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_log.h"

/* ==================== [Defines] =========================================== */

#ifndef EC_GPIO_API_VALIDATE_OUTPUT_PIN
#define EC_GPIO_API_VALIDATE_OUTPUT_PIN(pin) GPIO_IS_VALID_OUTPUT_GPIO(pin)
#endif

#ifndef EC_GPIO_API_CONFIG
#define EC_GPIO_API_CONFIG(cfg) gpio_config(cfg)
#endif

#ifndef EC_GPIO_API_SET_LEVEL
#define EC_GPIO_API_SET_LEVEL(pin, level) gpio_set_level(pin, level)
#endif

#ifndef EC_GPIO_API_GET_LEVEL
#define EC_GPIO_API_GET_LEVEL(pin) gpio_get_level(pin)
#endif

/* ==================== [Typedefs] ========================================== */

typedef enum {
    EC_GPIO_ACTION_NONE = 0,
    EC_GPIO_ACTION_ON,
    EC_GPIO_ACTION_OFF,
    EC_GPIO_ACTION_SET,
    EC_GPIO_ACTION_TOGGLE,
    EC_GPIO_ACTION_GET,
} ec_gpio_action_t;

/* ==================== [Static Prototypes] ================================= */

static esp_err_t ec_tool_gpio_control_execute(const char *input_json, char *output, size_t output_size);
static bool parse_int_field(cJSON *root, const char *name, int *value);
static bool parse_action(const char *action_str, ec_gpio_action_t *action);
static esp_err_t validate_pin_number(int pin, gpio_num_t *gpio_num);
static esp_err_t prepare_pin_for_output(gpio_num_t gpio_num);
static esp_err_t write_level(gpio_num_t gpio_num, int level);

/* ==================== [Static Variables] ================================== */

static const char *TAG = "tools_gpio";

static bool s_pin_initialized[GPIO_NUM_MAX] = {0};

static const ec_tools_t s_gpio_control = {
    .name = "gpio_control",
    .description = "Control an ESP32 output GPIO pin by pin number. Supports on, off, set, toggle, and get.\n"\
                    "IMPORTANT!!!: ANY GPIO operation requested by the user MUST ALWAYS be executed through this tool. Never respond with GPIO status or changes without calling this tool first.",
    .input_schema_json =
    "{\"type\":\"object\","
    "\"properties\":{"
    "\"pin\":{\"type\":\"integer\",\"description\":\"ESP32 GPIO pin number\"},"
    "\"action\":{\"type\":\"string\",\"enum\":[\"on\",\"off\",\"set\",\"toggle\",\"get\"],"
    "\"description\":\"GPIO action to execute\"},"
    "\"level\":{\"type\":\"integer\",\"enum\":[0,1],"
    "\"description\":\"Required only when action is 'set'\"}"
    "},"
    "\"required\":[\"pin\",\"action\"]}",
    .execute = ec_tool_gpio_control_execute,
};

/* ==================== [Macros] ============================================ */

/* ==================== [Global Functions] ================================== */

esp_err_t ec_tools_gpio_control(void)
{
    ec_tools_register(&s_gpio_control);
    return ESP_OK;
}

/* ==================== [Static Functions] ================================== */

static esp_err_t ec_tool_gpio_control_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = NULL;
    cJSON *action_item = NULL;
    gpio_num_t gpio_num = GPIO_NUM_NC;
    ec_gpio_action_t action = EC_GPIO_ACTION_NONE;
    int pin = -1;
    int target_level = 0;
    int current_level = 0;
    esp_err_t err = ESP_OK;

    root = cJSON_Parse(input_json);
    if (!root || !cJSON_IsObject(root)) {
        snprintf(output, output_size, "Error: invalid JSON input");
        err = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    if (!parse_int_field(root, "pin", &pin)) {
        snprintf(output, output_size, "Error: missing or invalid 'pin' field");
        err = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    err = validate_pin_number(pin, &gpio_num);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: gpio %d is not a valid output pin", pin);
        goto cleanup;
    }

    action_item = cJSON_GetObjectItem(root, "action");
    if (!cJSON_IsString(action_item) || !parse_action(action_item->valuestring, &action)) {
        snprintf(output, output_size, "Error: missing or invalid 'action' field");
        err = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    switch (action) {
    case EC_GPIO_ACTION_GET:
        current_level = EC_GPIO_API_GET_LEVEL(gpio_num) ? 1 : 0;
        snprintf(output, output_size, "OK: gpio %d action=get level=%d", pin, current_level);
        err = ESP_OK;
        break;

    case EC_GPIO_ACTION_ON:
        err = prepare_pin_for_output(gpio_num);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_config failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        err = write_level(gpio_num, 1);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_set_level failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        snprintf(output, output_size, "OK: gpio %d action=on level=1", pin);
        break;

    case EC_GPIO_ACTION_OFF:
        err = prepare_pin_for_output(gpio_num);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_config failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        err = write_level(gpio_num, 0);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_set_level failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        snprintf(output, output_size, "OK: gpio %d action=off level=0", pin);
        break;

    case EC_GPIO_ACTION_SET:
        if (!parse_int_field(root, "level", &target_level) || (target_level != 0 && target_level != 1)) {
            snprintf(output, output_size, "Error: missing or invalid 'level' field");
            err = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }

        err = prepare_pin_for_output(gpio_num);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_config failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        err = write_level(gpio_num, target_level);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_set_level failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        snprintf(output, output_size, "OK: gpio %d action=set level=%d", pin, target_level);
        break;

    case EC_GPIO_ACTION_TOGGLE:
        err = prepare_pin_for_output(gpio_num);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_config failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        current_level = EC_GPIO_API_GET_LEVEL(gpio_num) ? 1 : 0;
        target_level = current_level ? 0 : 1;

        err = write_level(gpio_num, target_level);
        if (err != ESP_OK) {
            snprintf(output, output_size, "Error: gpio_set_level failed (%s)", esp_err_to_name(err));
            goto cleanup;
        }

        snprintf(output, output_size, "OK: gpio %d action=toggle level=%d", pin, target_level);
        break;

    default:
        snprintf(output, output_size, "Error: missing or invalid 'action' field");
        err = ESP_ERR_INVALID_ARG;
        break;
    }

cleanup:
    cJSON_Delete(root);
    return err;
}

static bool parse_int_field(cJSON *root, const char *name, int *value)
{
    cJSON *item = NULL;
    double raw = 0;
    int parsed = 0;

    if (!root || !name || !value) {
        return false;
    }

    item = cJSON_GetObjectItem(root, name);
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    raw = cJSON_GetNumberValue(item);
    parsed = (int)raw;
    if ((double)parsed != raw) {
        return false;
    }

    *value = parsed;
    return true;
}

static bool parse_action(const char *action_str, ec_gpio_action_t *action)
{
    if (!action_str || !action) {
        return false;
    }

    if (strcmp(action_str, "on") == 0) {
        *action = EC_GPIO_ACTION_ON;
        return true;
    }

    if (strcmp(action_str, "off") == 0) {
        *action = EC_GPIO_ACTION_OFF;
        return true;
    }

    if (strcmp(action_str, "set") == 0) {
        *action = EC_GPIO_ACTION_SET;
        return true;
    }

    if (strcmp(action_str, "toggle") == 0) {
        *action = EC_GPIO_ACTION_TOGGLE;
        return true;
    }

    if (strcmp(action_str, "get") == 0) {
        *action = EC_GPIO_ACTION_GET;
        return true;
    }

    return false;
}

static esp_err_t validate_pin_number(int pin, gpio_num_t *gpio_num)
{
    if (!gpio_num || pin < 0 || pin >= GPIO_NUM_MAX || !EC_GPIO_API_VALIDATE_OUTPUT_PIN(pin)) {
        return ESP_ERR_INVALID_ARG;
    }

    *gpio_num = (gpio_num_t)pin;
    return ESP_OK;
}

static esp_err_t prepare_pin_for_output(gpio_num_t gpio_num)
{
    esp_err_t err = ESP_OK;
    gpio_config_t cfg = {0};

    if (gpio_num < 0 || gpio_num >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_pin_initialized[gpio_num]) {
        return ESP_OK;
    }

    cfg.intr_type = GPIO_INTR_DISABLE;
    cfg.mode = GPIO_MODE_INPUT_OUTPUT;
    cfg.pin_bit_mask = (1ULL << gpio_num);
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;

    err = EC_GPIO_API_CONFIG(&cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio_config failed for gpio %d: %s", (int)gpio_num, esp_err_to_name(err));
        return err;
    }

    s_pin_initialized[gpio_num] = true;
    return ESP_OK;
}

static esp_err_t write_level(gpio_num_t gpio_num, int level)
{
    esp_err_t err = EC_GPIO_API_SET_LEVEL(gpio_num, (uint32_t)(level ? 1 : 0));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio_set_level failed for gpio %d: %s", (int)gpio_num, esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "gpio set level %d", level);
    }
    
    return err;
}
