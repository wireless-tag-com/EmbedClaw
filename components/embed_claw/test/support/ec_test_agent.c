#include "ec_test_hooks.h"

#define ec_agent_start ec_agent_start__test_impl
#define ec_agent_inbound ec_agent_inbound__test_impl
#define ec_agent_outbound ec_agent_outbound__test_impl
#include "../../core/ec_agent.c"
#undef ec_agent_outbound
#undef ec_agent_inbound
#undef ec_agent_start

char *ec_agent_patch_tool_input_with_context_for_test(const char *tool_name, const char *input_json,
                                                      const ec_msg_t *msg)
{
    ec_llm_tool_call_t call = {0};

    if (tool_name) {
        strncpy(call.name, tool_name, sizeof(call.name) - 1);
    }
    call.input = (char *)(input_json ? input_json : "{}");
    call.input_len = input_json ? strlen(input_json) : 2;

    return patch_tool_input_with_context(&call, msg);
}

esp_err_t ec_agent_build_system_prompt_for_test(char *buf, size_t size)
{
    return context_build_system_prompt(buf, size);
}

void ec_agent_append_turn_context_for_test(char *prompt, size_t size, const ec_msg_t *msg)
{
    append_turn_context_prompt(prompt, size, msg);
}
