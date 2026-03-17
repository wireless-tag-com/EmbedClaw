#include <stdio.h>
#include <string.h>
#include <time.h>

#include "memory_checks.h"
#include "unity.h"

#include "core/ec_tools.h"
#include "support/ec_test_hooks.h"

static void register_tools_for_cron_tests(void)
{
    ec_tools_free_json();

    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_register_all());
    test_utils_record_free_mem();
}

static void cleanup_tools_after_test(void)
{
    ec_tools_free_json();
    
}

static void extract_job_id(const char *text, char *job_id, size_t job_id_size)
{
    const char *id = strstr(text, "id=");
    TEST_ASSERT_NOT_NULL(id);
    TEST_ASSERT_TRUE(job_id_size >= 9);
    memcpy(job_id, id + 3, 8);
    job_id[8] = '\0';
}

TEST_CASE("cron tool validates required fields and schedule arguments", "[embed_claw][tools][cron]")
{
    char output[256];
    char input[256];

    register_tools_for_cron_tests();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_tools_execute("cron_add", "{}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "missing required fields"));

    snprintf(input, sizeof(input),
             "{\"name\":\"once\",\"schedule_type\":\"at\",\"message\":\"ping\",\"at_epoch\":%lld}",
             (long long)time(NULL) - 1);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_tools_execute("cron_add", input, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "in the past"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_execute("cron_add",
                                       "{\"name\":\"relay\",\"schedule_type\":\"every\",\"interval_s\":60,"
                                       "\"message\":\"ping\",\"channel\":\"feishu\"}",
                                       output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "require both chat_type and chat_id"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_execute("cron_add",
                                       "{\"name\":\"qq relay\",\"schedule_type\":\"every\",\"interval_s\":60,"
                                       "\"message\":\"ping\",\"channel\":\"qq\",\"chat_id\":\"GROUP123\"}",
                                       output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "require both chat_type and chat_id"));

    cleanup_tools_after_test();
}

TEST_CASE("cron tool can add list and remove recurring jobs", "[embed_claw][tools][cron]")
{
    char input[256];
    char output[768];
    char job_id[16];
    char relay_job_id[16];

    register_tools_for_cron_tests();

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_execute("cron_add",
                                       "{\"name\":\"heartbeat\",\"schedule_type\":\"every\","
                                       "\"interval_s\":60,\"message\":\"ping\"}",
                                       output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "Added recurring job"));
    extract_job_id(output, job_id, sizeof(job_id));

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_execute("cron_add",
                                       "{\"name\":\"qq relay\",\"schedule_type\":\"every\","
                                       "\"interval_s\":60,\"message\":\"relay\","
                                       "\"channel\":\"qq\",\"chat_type\":\"group\",\"chat_id\":\"GROUP123\"}",
                                       output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "Added recurring job"));
    extract_job_id(output, relay_job_id, sizeof(relay_job_id));

    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("cron_list", "{}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "heartbeat"));
    TEST_ASSERT_NOT_NULL(strstr(output, "qq relay"));
    TEST_ASSERT_NOT_NULL(strstr(output, "system:cron"));
    TEST_ASSERT_NOT_NULL(strstr(output, "qq:group:GROUP123"));

    snprintf(input, sizeof(input), "{\"job_id\":\"%s\"}", job_id);
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("cron_remove", input, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "Removed cron job"));

    snprintf(input, sizeof(input), "{\"job_id\":\"%s\"}", relay_job_id);
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("cron_remove", input, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "Removed cron job"));

    cleanup_tools_after_test();
}

TEST_CASE("cron tool reports unknown remove target", "[embed_claw][tools][cron]")
{
    char output[128];

    register_tools_for_cron_tests();

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      ec_tools_execute("cron_remove", "{\"job_id\":\"deadbeef\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "not found"));

    cleanup_tools_after_test();
}
