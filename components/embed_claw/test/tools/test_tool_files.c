#include <string.h>
#include <stdio.h>

#include "unity.h"

#include "core/ec_tools.h"
#include "ec_config_internal.h"
#include "support/ec_test_hooks.h"

static void register_tools_for_file_tests(void)
{
    ec_tools_free_json();

    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_register_all());
}

static void cleanup_tools_after_test(void)
{
    ec_tools_free_json();
    
}

static void remove_if_exists(const char *path)
{
    (void)remove(path);
}

TEST_CASE("file tools validate sandboxed paths", "[embed_claw][tools][files]")
{
    TEST_ASSERT_TRUE(ec_tools_files_validate_path_for_test("/spiffs/config/user.txt"));
    TEST_ASSERT_TRUE(ec_tools_files_validate_path_for_test("/spiffs/memory/MEMORY.md"));

    TEST_ASSERT_FALSE(ec_tools_files_validate_path_for_test("/tmp/user.txt"));
    TEST_ASSERT_FALSE(ec_tools_files_validate_path_for_test("/spiffs/../secret.txt"));
    TEST_ASSERT_FALSE(ec_tools_files_validate_path_for_test("/spiffs.."));
}

TEST_CASE("file tools replace first occurrence only", "[embed_claw][tools][files]")
{
    char output[128];

    TEST_ASSERT_EQUAL(ESP_OK,
                      ec_tools_files_replace_first_for_test("hello world world", "world", "esp",
                                                            output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("hello esp world", output);
}

TEST_CASE("file tools report replace misses and invalid tool input", "[embed_claw][tools][files]")
{
    char output[128];

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      ec_tools_files_replace_first_for_test("hello", "missing", "esp", output, sizeof(output)));

    register_tools_for_file_tests();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_tools_execute("read_file", "{}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "path must start"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_execute("write_file", "{\"path\":\"/spiffs/demo.txt\"}", output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "missing 'content'"));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ec_tools_execute("edit_file", "{\"path\":\"/spiffs/demo.txt\",\"old_string\":\"a\"}",
                                       output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "missing 'old_string' or 'new_string'"));

    cleanup_tools_after_test();
}

TEST_CASE("file tools can write read edit and list files", "[embed_claw][tools][files]")
{
    const char *path = EC_FS_BASE "/unit-file-tools/demo.txt";
    char input[256];
    char output[512];

    TEST_ASSERT_EQUAL(ESP_OK, ec_test_spiffs_mount());
    remove_if_exists(path);
    register_tools_for_file_tests();

    snprintf(input, sizeof(input), "{\"path\":\"%s\",\"content\":\"alpha\"}", path);
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("write_file", input, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "OK: wrote"));

    snprintf(input, sizeof(input), "{\"path\":\"%s\"}", path);
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("read_file", input, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("alpha", output);

    snprintf(input, sizeof(input),
             "{\"path\":\"%s\",\"old_string\":\"alpha\",\"new_string\":\"beta\"}",
             path);
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("edit_file", input, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, "OK: edited"));

    snprintf(input, sizeof(input), "{\"path\":\"%s\"}", path);
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("read_file", input, output, sizeof(output)));
    TEST_ASSERT_EQUAL_STRING("beta", output);

    snprintf(input, sizeof(input), "{\"prefix\":\"" EC_FS_BASE "/unit-file-tools\"}");
    TEST_ASSERT_EQUAL(ESP_OK, ec_tools_execute("list_dir", input, output, sizeof(output)));
    TEST_ASSERT_NOT_NULL(strstr(output, path));

    cleanup_tools_after_test();
    remove_if_exists(path);
    ec_test_spiffs_unmount();
}
