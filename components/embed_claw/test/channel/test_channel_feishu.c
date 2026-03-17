#include <string.h>

#include "unity.h"

#include "support/ec_test_hooks.h"

TEST_CASE("feishu channel ping frames round-trip through parser", "[embed_claw][channel][feishu]")
{
    uint8_t buf[128];
    char type[32] = {0};
    char payload[64] = {0};
    int32_t method = -1;
    int32_t service = 0;
    uint64_t seq_id = 0;
    uint64_t log_id = 0;
    int n;

    n = ec_channel_feishu_encode_ping_for_test(buf, sizeof(buf), 321);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(ec_channel_feishu_parse_frame_for_test(buf, (size_t)n, &method, &service,
                                                            &seq_id, &log_id,
                                                            type, sizeof(type),
                                                            payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(0, method);
    TEST_ASSERT_EQUAL(321, service);
    TEST_ASSERT_EQUAL_STRING("ping", type);
}

TEST_CASE("feishu channel response frames preserve ids and payload", "[embed_claw][channel][feishu]")
{
    uint8_t buf[256];
    char payload[64] = {0};
    int32_t method = -1;
    int32_t service = 0;
    uint64_t seq_id = 0;
    uint64_t log_id = 0;
    int n;

    n = ec_channel_feishu_encode_response_for_test(buf, sizeof(buf), 11, 22, 33);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(ec_channel_feishu_parse_frame_for_test(buf, (size_t)n, &method, &service,
                                                            &seq_id, &log_id,
                                                            NULL, 0,
                                                            payload, sizeof(payload)));
    TEST_ASSERT_EQUAL(1, method);
    TEST_ASSERT_EQUAL(33, service);
    TEST_ASSERT_EQUAL(11, (unsigned int)seq_id);
    TEST_ASSERT_EQUAL(22, (unsigned int)log_id);
    TEST_ASSERT_EQUAL_STRING("{\"code\":200}", payload);
}
