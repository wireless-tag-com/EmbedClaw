#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "core/ec_tools.h"
#include "support/ec_test_hooks.h"


static void cleanup_tools_after_test(void)
{
    ec_tools_free_json();
    
}

static void register_builtin_tools_for_test(void)
{
    ec_tools_free_json();

    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_register_all());
}

TEST_CASE("tool registry builds json for built-in tools", "[embed_claw][tools][catalog]")
{
    register_builtin_tools_for_test();

    const char *json = ec_tools_get_json();
    TEST_ASSERT_NOT_NULL(json);
    TEST_ASSERT_NOT_NULL(strstr(json, "\"web_search\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"get_current_time\""));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"cron_add\""));
    cleanup_tools_after_test();
}

TEST_CASE("tool registry reports unknown tool without dereferencing null slots", "[embed_claw][tools][contract]")
{
    char output[128];

    ec_tools_free_json();
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ec_tools_execute("missing_tool", "{}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "unknown tool"));
    cleanup_tools_after_test();
}
