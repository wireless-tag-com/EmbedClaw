#include "ec_test_hooks.h"

#define ec_tools_web_search ec_tools_web_search__test_impl
#include "../../tools/tools_web_search.c"
#undef ec_tools_web_search

void ec_tools_web_search_format_results_for_test(const char *response_json, char *output, size_t output_size)
{
    cJSON *root;

    if (!output || output_size == 0) {
        return;
    }

    if (!response_json) {
        snprintf(output, output_size, "No web results found.");
        return;
    }

    root = cJSON_Parse(response_json);
    if (!root) {
        snprintf(output, output_size, "No web results found.");
        return;
    }

    format_results_tavily(root, output, output_size);
    cJSON_Delete(root);
}
