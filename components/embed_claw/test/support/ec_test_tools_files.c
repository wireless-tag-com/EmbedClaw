#include "ec_test_hooks.h"

#define ec_tools_read_file ec_tools_read_file__test_impl
#define ec_tools_write_file ec_tools_write_file__test_impl
#define ec_tools_edit_file ec_tools_edit_file__test_impl
#define ec_tools_list_dir ec_tools_list_dir__test_impl
#include "../../tools/tools_files.c"
#undef ec_tools_list_dir
#undef ec_tools_edit_file
#undef ec_tools_write_file
#undef ec_tools_read_file

bool ec_tools_files_validate_path_for_test(const char *path)
{
    return validate_path(path);
}

esp_err_t ec_tools_files_replace_first_for_test(const char *source, const char *old_str,
                                                const char *new_str, char *output, size_t output_size)
{
    return replace_first_occurrence(source, old_str, new_str, output, output_size);
}
