#include <string.h>
#include <stdlib.h>

#include "unity.h"

#include "support/ec_test_hooks.h"

TEST_CASE("ws channel parses websocket user payloads", "[embed_claw][channel][ws]")
{
    ec_msg_t msg = {0};

    ec_channel_ws_reset_state_for_test();
    ec_channel_ws_add_client_for_test(42, NULL);

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_channel_ws_parse_payload_for_test(42,
                              "{\"type\":\"message\",\"content\":\"hello\"}",
                              &msg));
    TEST_ASSERT_EQUAL_STRING("ws", msg.channel);
    TEST_ASSERT_EQUAL_STRING("ws_42", msg.chat_id);
    TEST_ASSERT_EQUAL_STRING("hello", msg.content);
    free(msg.content);
}

TEST_CASE("ws channel parses external relay payloads and validates chat id", "[embed_claw][channel][ws]")
{
    ec_msg_t msg = {0};

    ec_channel_ws_reset_state_for_test();

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_channel_ws_parse_payload_for_test(
                          7,
                          "{\"type\":\"message\",\"channel\":\"feishu\","
                          "\"chat_type\":\"open_id\",\"chat_id\":\"ou_123\",\"content\":\"nihao\"}",
                          &msg));
    TEST_ASSERT_EQUAL_STRING("feishu", msg.channel);
    TEST_ASSERT_EQUAL_STRING("open_id", msg.chat_type);
    TEST_ASSERT_EQUAL_STRING("ou_123", msg.chat_id);
    TEST_ASSERT_EQUAL_STRING("nihao", msg.content);
    free(msg.content);

    memset(&msg, 0, sizeof(msg));
    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_channel_ws_parse_payload_for_test(
                          7,
                          "{\"type\":\"message\",\"channel\":\"qq\","
                          "\"chat_type\":\"group\",\"chat_id\":\"GROUP123\",\"content\":\"relay\"}",
                          &msg));
    TEST_ASSERT_EQUAL_STRING("qq", msg.channel);
    TEST_ASSERT_EQUAL_STRING("group", msg.chat_type);
    TEST_ASSERT_EQUAL_STRING("GROUP123", msg.chat_id);
    TEST_ASSERT_EQUAL_STRING("relay", msg.content);
    free(msg.content);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_channel_ws_parse_payload_for_test(
                          7,
                          "{\"type\":\"message\",\"channel\":\"feishu\","
                          "\"chat_type\":\"open_id\",\"content\":\"nihao\"}",
                          &msg));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_channel_ws_parse_payload_for_test(
                          7,
                          "{\"type\":\"message\",\"channel\":\"qq\","
                          "\"chat_id\":\"GROUP123\",\"content\":\"relay\"}",
                          &msg));
}

TEST_CASE("ws channel rejects invalid payloads and builds response json", "[embed_claw][channel][ws]")
{
    char pong[] = "pong";
    char output[128];
    ec_msg_t msg = {0};

    ec_channel_ws_reset_state_for_test();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_channel_ws_parse_payload_for_test(1, "{", &msg));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      ec_channel_ws_parse_payload_for_test(1, "{\"type\":\"ping\",\"content\":\"hello\"}", &msg));

    strncpy(msg.chat_id, "ws_99", sizeof(msg.chat_id) - 1);
    msg.content = pong;
    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_ws_build_response_json_for_test(&msg, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"type\":\"response\""));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"content\":\"pong\""));
    TEST_ASSERT_NOT_NULL(strstr(output, "\"chat_id\":\"ws_99\""));
}
