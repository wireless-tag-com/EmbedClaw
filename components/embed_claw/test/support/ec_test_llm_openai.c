#include "ec_test_hooks.h"

#define ec_llm_openai_get_provider ec_llm_openai_get_provider__test_impl
#include "../../llm/ec_llm_openai.c"
#undef ec_llm_openai_get_provider

cJSON *ec_llm_openai_convert_tools_for_test(const char *tools_json)
{
    return convert_tools_openai(tools_json);
}

cJSON *ec_llm_openai_convert_messages_for_test(const char *system_prompt, cJSON *messages)
{
    return convert_messages_openai(system_prompt, messages);
}

const char *ec_llm_openai_select_server_ca_pem_for_test(const char *url)
{
    return select_server_ca_pem(url);
}
