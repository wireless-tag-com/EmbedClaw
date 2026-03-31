#include <string.h>

#include "unity.h"

#include "core/ec_tools.h"
#include "support/ec_test_hooks.h"

static void reset_gpio_test_state(void)
{
    ec_tools_free_json();
    ec_tools_gpio_reset_for_test();
}

static void register_builtin_tools_for_test(void)
{
    reset_gpio_test_state();
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_register_all());
}

TEST_CASE("gpio tool rejects invalid input and unsupported requests", "[embed_claw][tools][gpio]")
{
    char output[160];

    reset_gpio_test_state();
    ec_tools_gpio_set_valid_output_for_test(18, true);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "invalid JSON"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "'pin'"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{\"pin\":18}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "'action'"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"blink\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "'action'"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"set\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "'level'"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"set\",\"level\":2}",
                                                     output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "'level'"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_gpio_execute_for_test("{\"pin\":46,\"action\":\"get\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "not a valid output pin"));
}

TEST_CASE("gpio tool supports get on off set and toggle with cached configuration", "[embed_claw][tools][gpio]")
{
    char output[160];

    reset_gpio_test_state();
    ec_tools_gpio_set_valid_output_for_test(18, true);

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"get\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=get level=0"));
    TEST_ASSERT_EQUAL(0, ec_tools_gpio_get_config_call_count_for_test(18));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"on\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=on level=1"));
    TEST_ASSERT_EQUAL(1, ec_tools_gpio_get_config_call_count_for_test(18));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"get\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=get level=1"));
    TEST_ASSERT_EQUAL(1, ec_tools_gpio_get_config_call_count_for_test(18));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"off\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=off level=0"));
    TEST_ASSERT_EQUAL(1, ec_tools_gpio_get_config_call_count_for_test(18));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"set\",\"level\":1}",
                                                     output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=set level=1"));
    TEST_ASSERT_EQUAL(1, ec_tools_gpio_get_config_call_count_for_test(18));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"toggle\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=toggle level=0"));
    TEST_ASSERT_EQUAL(1, ec_tools_gpio_get_config_call_count_for_test(18));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"get\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "action=get level=0"));
}

TEST_CASE("gpio tool formats driver failures", "[embed_claw][tools][gpio]")
{
    char output[160];

    reset_gpio_test_state();
    ec_tools_gpio_set_valid_output_for_test(18, true);
    ec_tools_gpio_set_driver_failures_for_test(ESP_FAIL, ESP_OK);

    TEST_ASSERT_EQUAL(ESP_FAIL,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"on\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "gpio_config failed"));
    TEST_ASSERT_EQUAL(0, ec_tools_gpio_get_config_call_count_for_test(18));

    reset_gpio_test_state();
    ec_tools_gpio_set_valid_output_for_test(18, true);
    ec_tools_gpio_set_driver_failures_for_test(ESP_OK, ESP_ERR_INVALID_STATE);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      ec_tools_gpio_execute_for_test("{\"pin\":18,\"action\":\"on\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "gpio_set_level failed"));
    TEST_ASSERT_EQUAL(1, ec_tools_gpio_get_config_call_count_for_test(18));
}

TEST_CASE("gpio tool is registered in builtin tool catalog", "[embed_claw][tools][gpio]")
{
    const char *json = NULL;

    register_builtin_tools_for_test();
    json = ec_tools_get_json();

    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"gpio_control\""));

    reset_gpio_test_state();
}
