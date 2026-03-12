#include "ec_test_hooks.h"

#define ec_tools_get_time ec_tools_get_time__test_impl
#include "../../tools/tools_get_time.c"
#undef ec_tools_get_time

bool ec_tools_get_time_format_epoch_for_test(time_t epoch, char *out, size_t out_size)
{
    return format_epoch(epoch, out, out_size);
}
