#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

#include "ec_config_internal.h"
#include "core/ec_channel.h"
#include "core/ec_skill_loader.h"
#include "support/ec_test_hooks.h"

static void remove_if_exists(const char *path)
{
    (void)remove(path);
}

static void daily_note_path(char *buf, size_t size)
{
    time_t now = time(NULL);
    struct tm local = {0};
    char date_str[16];

    localtime_r(&now, &local);
    strftime(date_str, sizeof(date_str), "%Y-%m-%d", &local);
    snprintf(buf, size, "%s/%s.md", EC_FS_MEMORY_DIR, date_str);
}

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");

    TEST_ASSERT_NOT_NULL(f);
    fputs(content, f);
    fclose(f);
}

TEST_CASE("agent patches cron target from current turn context", "[embed_claw][core][agent]")
{
    ec_msg_t msg = {0};
    char *patched;

    strncpy(msg.channel, g_ec_channel_feishu, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, "open_id:ou_123", sizeof(msg.chat_id) - 1);

    patched = ec_agent_patch_tool_input_with_context_for_test(
        "cron_add",
        "{\"name\":\"heartbeat\",\"schedule_type\":\"every\",\"interval_s\":60,"
        "\"message\":\"ping\",\"channel\":\"feishu\"}",
        &msg);
    TEST_ASSERT_NOT_NULL(patched);
    TEST_ASSERT_NOT_NULL(strstr(patched, "\"channel\":\"feishu\""));
    TEST_ASSERT_NOT_NULL(strstr(patched, "\"chat_id\":\"open_id:ou_123\""));
    free(patched);

    patched = ec_agent_patch_tool_input_with_context_for_test(
        "cron_add",
        "{\"name\":\"heartbeat\",\"schedule_type\":\"every\",\"interval_s\":60,"
        "\"message\":\"ping\",\"channel\":\"feishu\",\"chat_id\":\"open_id:ou_fixed\"}",
        &msg);
    TEST_ASSERT_NULL(patched);

    patched = ec_agent_patch_tool_input_with_context_for_test(
        "read_file",
        "{\"path\":\"/spiffs/demo.txt\"}",
        &msg);
    TEST_ASSERT_NULL(patched);
}

TEST_CASE("agent system prompt includes files memory skills and turn context", "[embed_claw][core][agent]")
{
    const size_t prompt_size = 8192;
    char *prompt = malloc(prompt_size);
    char today_path[64];
    ec_msg_t msg = {0};

    TEST_ASSERT_NOT_NULL(prompt);
    TEST_ASSERT_EQUAL(ESP_OK, ec_test_spiffs_mount());

    daily_note_path(today_path, sizeof(today_path));
    remove_if_exists(EC_SOUL_FILE);
    remove_if_exists(EC_USER_FILE);
    remove_if_exists(EC_MEMORY_FILE);
    remove_if_exists(today_path);

    write_text_file(EC_SOUL_FILE, "Be concise.\n");
    write_text_file(EC_USER_FILE, "Name: Alice\n");
    write_text_file(EC_MEMORY_FILE, "favorite drink: coffee\n");
    write_text_file(today_path, "# Today\n\ntoday note\n");
    TEST_ASSERT_EQUAL(ESP_OK, ec_skill_loader_init());

    TEST_ASSERT_EQUAL(ESP_OK, ec_agent_build_system_prompt_for_test(prompt, prompt_size));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "You are EmbedClaw"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "## Personality"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "Be concise."));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "## User Info"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "Name: Alice"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "## Long-term Memory"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "favorite drink: coffee"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "## Recent Notes"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "today note"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "## Available Skills"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "Weather"));

    strncpy(msg.channel, g_ec_channel_ws, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, "ws_42", sizeof(msg.chat_id) - 1);
    ec_agent_append_turn_context_for_test(prompt, prompt_size, &msg);
    TEST_ASSERT_NOT_NULL(strstr(prompt, "source_channel: ws"));
    TEST_ASSERT_NOT_NULL(strstr(prompt, "source_chat_id: ws_42"));

    free(prompt);
    remove_if_exists(EC_SOUL_FILE);
    remove_if_exists(EC_USER_FILE);
    remove_if_exists(EC_MEMORY_FILE);
    remove_if_exists(today_path);
    ec_test_spiffs_unmount();
}
