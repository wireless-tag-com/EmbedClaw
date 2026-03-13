#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "core/ec_channel.h"
#include "support/ec_test_hooks.h"

static int s_fake_start_calls;
static int s_fake_send_calls;
static const char *s_last_content;

static esp_err_t fake_channel_start(void)
{
    s_fake_start_calls++;
    return ESP_OK;
}

static esp_err_t fake_channel_send(const ec_msg_t *msg)
{
    s_fake_send_calls++;
    s_last_content = msg ? msg->content : NULL;
    return ESP_OK;
}

static const ec_channel_t s_fake_channel = {
    .name = "fake_channel",
    .vtable = {
        .start = fake_channel_start,
        .send = fake_channel_send,
    },
};

TEST_CASE("channel registry dispatches to registered driver", "[embed_claw][channel][contract]")
{
    char hello[] = "hello";

    s_fake_start_calls = 0;
    s_fake_send_calls = 0;
    s_last_content = NULL;

    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_register(&s_fake_channel));
    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_start());

    ec_msg_t msg = {0};
    strncpy(msg.channel, "fake_channel", sizeof(msg.channel) - 1);
    msg.content = hello;
    TEST_ASSERT_EQUAL(ESP_OK, ec_channel_send(&msg));

    TEST_ASSERT_EQUAL(1, s_fake_start_calls);
    TEST_ASSERT_EQUAL(1, s_fake_send_calls);
    TEST_ASSERT_EQUAL_PTR(hello, s_last_content);
}

TEST_CASE("channel send reports unknown driver", "[embed_claw][channel][contract]")
{
    char hello[] = "hello";

    ec_msg_t msg = {0};
    strncpy(msg.channel, "missing_channel", sizeof(msg.channel) - 1);
    msg.content = hello;

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, ec_channel_send(&msg));
}
