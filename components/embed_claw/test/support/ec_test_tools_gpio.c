#include <string.h>

#include "driver/gpio.h"
#include "ec_test_hooks.h"

static bool ec_test_gpio_is_valid_output(int pin);
static esp_err_t ec_test_gpio_config(const gpio_config_t *cfg);
static esp_err_t ec_test_gpio_set_level(gpio_num_t pin, uint32_t level);
static int ec_test_gpio_get_level(gpio_num_t pin);

#define EC_GPIO_API_VALIDATE_OUTPUT_PIN(pin) ec_test_gpio_is_valid_output((pin))
#define EC_GPIO_API_CONFIG(cfg) ec_test_gpio_config((cfg))
#define EC_GPIO_API_SET_LEVEL(pin, level) ec_test_gpio_set_level((pin), (level))
#define EC_GPIO_API_GET_LEVEL(pin) ec_test_gpio_get_level((pin))
#define ec_tools_gpio_control ec_tools_gpio_control__test_impl
#include "../../tools/tools_gpio.c"
#undef ec_tools_gpio_control
#undef EC_GPIO_API_GET_LEVEL
#undef EC_GPIO_API_SET_LEVEL
#undef EC_GPIO_API_CONFIG
#undef EC_GPIO_API_VALIDATE_OUTPUT_PIN

static bool s_fake_valid_output[GPIO_NUM_MAX];
static int s_fake_level[GPIO_NUM_MAX];
static int s_fake_config_calls[GPIO_NUM_MAX];
static esp_err_t s_fake_config_err = ESP_OK;
static esp_err_t s_fake_set_err = ESP_OK;

static bool ec_test_gpio_is_valid_output(int pin)
{
    return pin >= 0 && pin < GPIO_NUM_MAX && s_fake_valid_output[pin];
}

static esp_err_t ec_test_gpio_config(const gpio_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_fake_config_err != ESP_OK) {
        return s_fake_config_err;
    }

    for (int pin = 0; pin < GPIO_NUM_MAX && pin < 64; pin++) {
        if ((cfg->pin_bit_mask & (1ULL << pin)) != 0) {
            s_fake_config_calls[pin]++;
        }
    }

    return ESP_OK;
}

static esp_err_t ec_test_gpio_set_level(gpio_num_t pin, uint32_t level)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_fake_set_err != ESP_OK) {
        return s_fake_set_err;
    }

    s_fake_level[pin] = level ? 1 : 0;
    return ESP_OK;
}

static int ec_test_gpio_get_level(gpio_num_t pin)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return 0;
    }

    return s_fake_level[pin];
}

esp_err_t ec_tools_gpio_execute_for_test(const char *input_json, char *output, size_t output_size)
{
    return ec_tool_gpio_control_execute(input_json, output, output_size);
}

void ec_tools_gpio_reset_for_test(void)
{
    memset(s_fake_valid_output, 0, sizeof(s_fake_valid_output));
    memset(s_fake_level, 0, sizeof(s_fake_level));
    memset(s_fake_config_calls, 0, sizeof(s_fake_config_calls));
    memset(s_pin_initialized, 0, sizeof(s_pin_initialized));
    s_fake_config_err = ESP_OK;
    s_fake_set_err = ESP_OK;
}

void ec_tools_gpio_set_valid_output_for_test(int pin, bool valid)
{
    if (pin >= 0 && pin < GPIO_NUM_MAX) {
        s_fake_valid_output[pin] = valid;
    }
}

void ec_tools_gpio_set_driver_failures_for_test(esp_err_t config_err, esp_err_t set_err)
{
    s_fake_config_err = config_err;
    s_fake_set_err = set_err;
}

int ec_tools_gpio_get_config_call_count_for_test(int pin)
{
    if (pin < 0 || pin >= GPIO_NUM_MAX) {
        return 0;
    }

    return s_fake_config_calls[pin];
}
