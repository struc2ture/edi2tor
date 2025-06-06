#include <stdio.h>

#include "editor.h"
// #include "util.h"

void unit_tests_log_result(Text_Buffer *log_buffer, const char *unit_test_name, bool success)
{
    const char *succes_str = success ? "SUCCESS" : "FAILURE";
    text_buffer_append(log_buffer, "%s: %s", unit_test_name, succes_str);
}

void test_cursor_pos_to_start_of_buffer(Text_Buffer *log_buffer)
{
    Text_Buffer text_buffer = {0};
    text_buffer_append(&text_buffer, "Hello!!!");
    text_buffer_append(&text_buffer, "And good bye!!!");

    Cursor_Pos pos = {1, 0};
    pos = cursor_pos_to_start_of_buffer(text_buffer, pos);

    Cursor_Pos expected_pos = {0, 0};

    unit_tests_log_result(log_buffer,
        "test_cursor_pos_to_start_of_buffer",
        pos.line == expected_pos.line && pos.col == expected_pos.col);
}

void unit_tests_run(Text_Buffer *log_buffer)
{
    test_cursor_pos_to_start_of_buffer(log_buffer);
}
