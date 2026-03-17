#include <string.h>

#include "unity.h"

#include "core/ec_tools.h"
#include "support/ec_test_hooks.h"

static void register_tools_for_tool_tests(void)
{
    ec_tools_free_json();
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_register_all());
}

static void cleanup_tools_after_test(void)
{
    ec_tools_free_json();
    
}

TEST_CASE("web search tool rejects invalid json and missing query", "[embed_claw][tools][web_search]")
{
    char output[128];

    register_tools_for_tool_tests();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_tools_execute("web_search", "{", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "Invalid input JSON"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_tools_execute("web_search", "{}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "Missing 'query'"));

    cleanup_tools_after_test();
}

TEST_CASE("web search formatter renders tavily results", "[embed_claw][tools][web_search]")
{
    char output[512];
    const char *response_json =
        "{"
        "\"results\":["
        "{\"title\":\"Title A\",\"url\":\"https://a.example\",\"content\":\"Summary A\"},"
        "{\"title\":\"Title B\",\"url\":\"https://b.example\",\"content\":\"Summary B\"}"
        "]"
        "}";

    ec_tools_web_search_format_results_for_test(response_json, output, sizeof(output));

    TEST_ASSERT_NOT_NULL(strstr(output, "1. Title A"));
    TEST_ASSERT_NOT_NULL(strstr(output, "https://a.example"));
    TEST_ASSERT_NOT_NULL(strstr(output, "Summary B"));
}

TEST_CASE("web search formatter handles empty results", "[embed_claw][tools][web_search]")
{
    char output[128];

    ec_tools_web_search_format_results_for_test("{\"results\":[]}", output, sizeof(output));

    TEST_ASSERT_EQUAL_STRING("No web results found.", output);
}
