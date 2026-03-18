#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "cJSON.h"

#include "llm/ec_llm.h"
#include "support/ec_test_hooks.h"

TEST_CASE("llm layer rejects invalid setup and uninitialized use", "[embed_claw][llm][contract]")
{
    cJSON *messages = cJSON_CreateArray();
    ec_llm_response_t resp = {0};
    ec_llm_provider_ctx_t provider_ctx = {
        .url = "https://example.com",
        .api_key = "test",
        .model = "test",
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      ec_llm_chat_tools("system", messages, "[]", &resp));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ec_llm_init_default(NULL));
    TEST_ASSERT_EQUAL(ESP_OK, ec_llm_init_default(&provider_ctx));

    cJSON_Delete(messages);
}

TEST_CASE("llm response cleanup releases owned buffers", "[embed_claw][llm][contract]")
{
    ec_llm_response_t resp = {0};
    resp.text = strdup("hello");
    resp.text_len = 5;
    resp.call_count = 1;
    resp.tool_use = true;
    resp.calls[0].input = strdup("{\"ok\":true}");
    resp.calls[0].input_len = 11;

    TEST_ASSERT_EQUAL(ESP_OK, ec_llm_response_free(&resp));
    TEST_ASSERT_NULL(resp.text);
    TEST_ASSERT_NULL(resp.calls[0].input);
    TEST_ASSERT_EQUAL(0, resp.call_count);
    TEST_ASSERT_FALSE(resp.tool_use);
}
