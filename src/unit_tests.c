#include <stdio.h>

#include "editor.h"

typedef struct
{
    bool break_on_failure;
    Text_Buffer *log_buffer;
    int tests_run;
    int tests_succeeded;
} UT_State;

void unit_tests_run_check(UT_State *s, const char *test_name, bool success, const char *expr)
{
    const char *succes_str = success ? "SUCCESS" : "FAILURE";
    text_buffer_append(s->log_buffer, "[%s] %s:", succes_str, test_name);
    text_buffer_append(s->log_buffer, "    (%s)", expr);
    s->tests_run++;
    s->tests_succeeded += (int)success;
    if (!success && s->break_on_failure)
        __builtin_debugtrap();
}
#define UNIT_TESTS_RUN_CHECK(expr) unit_tests_run_check(s, __func__, (expr), #expr)

void unit_tests_finish(UT_State *s)
{
    text_buffer_append(s->log_buffer, "");
    text_buffer_append(s->log_buffer,
        "Finished: %d out of %d tests are successful.",
        s->tests_succeeded,
        s->tests_run);
}

void test_text_buffer_append_empty(UT_State *s)
{
    const char *str = "Hello!!!";
    int len = strlen(str);

    Text_Buffer text_buffer = {0};
    text_buffer_append(&text_buffer, "Hello!!!");

    bool one_line = text_buffer.line_count == 1;
    bool line_len = text_buffer.lines[0].len == len + 1;
    bool buf_len = text_buffer.lines[0].buf_len == len + 2;
    bool strs_equal = strncmp(str, text_buffer.lines[0].str, len) == 0;
    bool extra_new_line = text_buffer.lines[0].str[text_buffer.lines[0].len - 1] == '\n';
    bool null_terminator = text_buffer.lines[0].str[text_buffer.lines[0].len] == '\0';

    UNIT_TESTS_RUN_CHECK(one_line && line_len && buf_len && strs_equal && extra_new_line && null_terminator);
}

void test_cursor_pos_to_start_of_buffer_regular_buffer(UT_State *s)
{
    Text_Buffer text_buffer = {0};
    text_buffer_append(&text_buffer, "Hello!!!");
    text_buffer_append(&text_buffer, "And good bye!!!");

    Cursor_Pos pos = {1, 0};
    pos = cursor_pos_to_start_of_buffer(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(pos.line == 0 && pos.col == 0);
}

void unit_tests_run(Text_Buffer *log_buffer, bool break_on_failure)
{
    UT_State s = {0};
    s.log_buffer = log_buffer;
    s.break_on_failure = break_on_failure;

    test_text_buffer_append_empty(&s);

    test_cursor_pos_to_start_of_buffer_regular_buffer(&s);

    unit_tests_finish(&s);
}
