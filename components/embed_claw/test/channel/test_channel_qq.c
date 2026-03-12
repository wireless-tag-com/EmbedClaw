#include <string.h>
#include <stdlib.h>

#include "unity.h"

#include "support/ec_test_hooks.h"

TEST_CASE("qq channel parses private and group text events", "[embed_claw][channel][qq]")
{
    ec_msg_t msg = {0};

    ec_channel_qq_reset_state_for_test();

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_channel_qq_parse_event_for_test(
                          "{\"op\":0,\"t\":\"C2C_MESSAGE_CREATE\",\"d\":{"
                          "\"author\":{\"user_openid\":\"OPENID123\"},"
                          "\"content\":\"hello qq\",\"id\":\"m1\",\"timestamp\":\"2026-03-13T00:00:00Z\"}}",
                          &msg));
    TEST_ASSERT_EQUAL_STRING("qq", msg.channel);
    TEST_ASSERT_EQUAL_STRING("c2c:OPENID123", msg.chat_id);
    TEST_ASSERT_EQUAL_STRING("hello qq", msg.content);
    free(msg.content);

    memset(&msg, 0, sizeof(msg));
    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_channel_qq_parse_event_for_test(
                          "{\"op\":0,\"t\":\"GROUP_AT_MESSAGE_CREATE\",\"d\":{"
                          "\"group_openid\":\"GROUP123\","
                          "\"content\":\"hello group\",\"id\":\"m2\",\"timestamp\":\"2026-03-13T00:00:01Z\"}}",
                          &msg));
    TEST_ASSERT_EQUAL_STRING("qq", msg.channel);
    TEST_ASSERT_EQUAL_STRING("group:GROUP123", msg.chat_id);
    TEST_ASSERT_EQUAL_STRING("hello group", msg.content);
    free(msg.content);
}

TEST_CASE("qq channel ignores unsupported events and self messages", "[embed_claw][channel][qq]")
{
    ec_msg_t msg = {0};

    ec_channel_qq_reset_state_for_test();

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      ec_channel_qq_parse_event_for_test(
                          "{\"op\":10,\"d\":{\"heartbeat_interval\":30000}}",
                          &msg));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_channel_qq_parse_event_for_test(
                          "{\"op\":0,\"t\":\"C2C_MESSAGE_CREATE\",\"d\":{"
                          "\"author\":{},\"content\":\"loopback\"}}",
                          &msg));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      ec_channel_qq_parse_event_for_test(
                          "{\"op\":0,\"t\":\"MESSAGE_REACTION_ADD\",\"d\":{}}",
                          &msg));
}

TEST_CASE("qq channel builds outbound actions and validates chat ids", "[embed_claw][channel][qq]")
{
    char path[128];
    char body[256];
    char text[] = "pong";
    char kind[16];
    char id_buf[32];
    ec_msg_t msg = {0};

    ec_channel_qq_reset_state_for_test();

    TEST_ASSERT_TRUE(ec_channel_qq_parse_chat_id_for_test("c2c:OPENID123", kind, sizeof(kind),
                                                          id_buf, sizeof(id_buf)));
    TEST_ASSERT_EQUAL_STRING("c2c", kind);
    TEST_ASSERT_EQUAL_STRING("OPENID123", id_buf);

    TEST_ASSERT_TRUE(ec_channel_qq_parse_chat_id_for_test("group:GROUP123", kind, sizeof(kind),
                                                          id_buf, sizeof(id_buf)));
    TEST_ASSERT_EQUAL_STRING("group", kind);
    TEST_ASSERT_EQUAL_STRING("GROUP123", id_buf);

    TEST_ASSERT_TRUE(ec_channel_qq_parse_chat_id_for_test("channel:98765", kind, sizeof(kind),
                                                          id_buf, sizeof(id_buf)));
    TEST_ASSERT_EQUAL_STRING("channel", kind);
    TEST_ASSERT_EQUAL_STRING("98765", id_buf);

    strncpy(msg.chat_id, "c2c:OPENID123", sizeof(msg.chat_id) - 1);
    msg.content = text;
    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_qq_build_request_for_test(&msg, path, sizeof(path), body, sizeof(body)));
    TEST_ASSERT_EQUAL_STRING("/v2/users/OPENID123/messages", path);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"content\":\"pong\""));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"msg_type\":0"));
    TEST_ASSERT_NOT_NULL(strstr(body, "\"msg_seq\":1"));

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.chat_id, "group:GROUP123", sizeof(msg.chat_id) - 1);
    msg.content = text;
    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_qq_build_request_for_test(&msg, path, sizeof(path), body, sizeof(body)));
    TEST_ASSERT_EQUAL_STRING("/v2/groups/GROUP123/messages", path);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"content\":\"pong\""));

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.chat_id, "channel:98765", sizeof(msg.chat_id) - 1);
    msg.content = text;
    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_qq_build_request_for_test(&msg, path, sizeof(path), body, sizeof(body)));
    TEST_ASSERT_EQUAL_STRING("/channels/98765/messages", path);
    TEST_ASSERT_NOT_NULL(strstr(body, "\"content\":\"pong\""));
    TEST_ASSERT_NULL(strstr(body, "\"msg_type\""));

    memset(&msg, 0, sizeof(msg));
    strncpy(msg.chat_id, "qq_group:not_digits", sizeof(msg.chat_id) - 1);
    msg.content = text;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_channel_qq_build_request_for_test(&msg, path, sizeof(path), body, sizeof(body)));
}
