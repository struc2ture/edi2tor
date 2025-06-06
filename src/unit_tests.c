#include <stdio.h>

#include "editor.h"

typedef struct
{
    bool break_on_failure;
    Text_Buffer *log_buffer;
    int tests_run;
    int tests_succeeded;
} UT_State;

void _unit_tests_run_check(UT_State *s, const char *test_name, bool success, const char *expr)
{
    const char *succes_str = success ? "SUCCESS" : "FAILURE";
    text_buffer_append_f(s->log_buffer, "[%s] %s:", succes_str, test_name);
    text_buffer_append_f(s->log_buffer, "    (%s)", expr);
    s->tests_run++;
    s->tests_succeeded += (int)success;
    if (!success && s->break_on_failure)
        __builtin_debugtrap();
}
#define UNIT_TESTS_RUN_CHECK(expr) _unit_tests_run_check(s, __func__, (expr), #expr)

void _unit_tests_finish(UT_State *s)
{
    text_buffer_append_f(s->log_buffer, "");
    text_buffer_append_f(s->log_buffer,
        "Finished: %d out of %d tests are successful.",
        s->tests_succeeded,
        s->tests_run);
}

// ---------------------------------------------------------------------

void test__text_line_make_dup__regular_string(UT_State *s)
{
    char *str = "Hello!!!";
    int len = strlen(str);

    Text_Line text_line = text_line_make_dup(str);

    bool str_exists = text_line.str != NULL;
    bool different_pointers = text_line.str != str;
    bool equal_len = text_line.len == len;
    bool correct_buf_len = text_line.buf_len == len + 1;
    bool null_terminator = text_line.str[text_line.len] == '\0';
    bool str_equal = strcmp(text_line.str, str) == 0;

    UNIT_TESTS_RUN_CHECK(str_exists && different_pointers && equal_len && correct_buf_len && null_terminator && str_equal);

    free(text_line.str);
}

void test__text_line_make_dup__empty_string(UT_State *s)
{
    char *str = "";
    int len = strlen(str);

    Text_Line text_line = text_line_make_dup(str);

    bool str_exists = text_line.str != NULL;
    bool different_pointers = text_line.str != str;
    bool equal_len = text_line.len == len;
    bool correct_buf_len = text_line.buf_len == len + 1;
    bool null_terminator = text_line.str[text_line.len] == '\0';
    bool str_equal = strcmp(text_line.str, str) == 0;

    UNIT_TESTS_RUN_CHECK(str_exists && different_pointers && equal_len && correct_buf_len && null_terminator && str_equal);

    free(text_line.str);
}

void test__text_line_make_f__regular_string(UT_State *s)
{
    char *str = "Hello 123 0.3\n";
    int len = strlen(str);

    Text_Line text_line = text_line_make_f("%s %d %.1f", "Hello", 123, 0.3f);

    bool str_exists = text_line.str != NULL;
    bool equal_len = text_line.len == len;
    bool correct_buf_len = text_line.buf_len == len + 1;
    bool null_terminator = text_line.str[text_line.len] == '\0';
    bool str_equal = strcmp(text_line.str, str) == 0;

    UNIT_TESTS_RUN_CHECK(str_exists && equal_len && correct_buf_len && null_terminator && str_equal);

    free(text_line.str);
}

void test__text_line_copy__whole(UT_State *s)
{
    char *str = "Hello 123 0.3\n";
    Text_Line original = text_line_make_dup(str);

    Text_Line copied = text_line_copy(original, 0, -1);

    bool str_exists = copied.str != NULL;
    bool different_pointers = copied.str != original.str;
    bool equal_len = copied.len == original.len;
    bool equal_buf_len = copied.buf_len == original.buf_len;
    bool null_terminator = copied.str[copied.len] == '\0';
    bool str_equal = strcmp(copied.str, original.str) == 0;

    UNIT_TESTS_RUN_CHECK(str_exists && different_pointers && equal_len && equal_buf_len && null_terminator && str_equal);
}

void test__text_line_copy__range(UT_State *s)
{
    char *str = "Hello 123 0.3\n";
    Text_Line original = text_line_make_dup(str);

    Text_Line copied = text_line_copy(original, 6, 9);

    bool str_exists = copied.str != NULL;
    bool different_pointers = copied.str != original.str;
    bool correct_len = copied.len == 3;
    bool correct_buf_len = copied.buf_len == 4;
    bool null_terminator = copied.str[copied.len] == '\0';
    bool correct_str = strcmp(copied.str, "123") == 0;

    UNIT_TESTS_RUN_CHECK(str_exists && different_pointers && correct_len && correct_buf_len && null_terminator && correct_str);
}

void test__text_line_resize__shorten(UT_State *s)
{
    char *str = "Test string 123456789";
    int resize_to = 4;
    Text_Line text_line = text_line_make_dup(str);

    text_line_resize(&text_line, resize_to);

    bool correct_len = text_line.len == resize_to;
    bool correct_buf_len = text_line.buf_len == resize_to + 1;
    bool null_terminator = text_line.str[text_line.len] == '\0';
    bool correct_str = strcmp(text_line.str, "Test") == 0;

    UNIT_TESTS_RUN_CHECK(correct_len && correct_buf_len && null_terminator && correct_str);
}

void test__text_line_resize__lengthen(UT_State *s)
{
    char *str = "Test string 123456789";
    int len = 21;
    int resize_to = 25;
    Text_Line text_line = text_line_make_dup(str);

    text_line_resize(&text_line, resize_to);

    bool correct_len = text_line.len == resize_to;
    bool correct_buf_len = text_line.buf_len == resize_to + 1;
    bool null_terminator = text_line.str[text_line.len] == '\0';
    bool correct_str = strncmp(text_line.str, str, len) == 0;

    UNIT_TESTS_RUN_CHECK(correct_len && correct_buf_len && null_terminator && correct_str);
}

void test__cursor_pos_to_start_of_buffer__regular_buffer(UT_State *s)
{
    Text_Buffer text_buffer = {0};
    text_buffer_append_f(&text_buffer, "Hello!!!");
    text_buffer_append_f(&text_buffer, "And good bye!!!");

    Cursor_Pos pos = {1, 0};
    pos = cursor_pos_to_start_of_buffer(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(pos.line == 0 && pos.col == 0);
}

// ---------------------------------------------------------------------

void unit_tests_run(Text_Buffer *log_buffer, bool break_on_failure)
{
    UT_State s = {0};
    s.log_buffer = log_buffer;
    s.break_on_failure = break_on_failure;

    text_buffer_append_f(s.log_buffer, "TEXT LINE TESTS:");
    test__text_line_make_dup__regular_string(&s);
    test__text_line_make_dup__empty_string(&s);
    test__text_line_make_f__regular_string(&s);
    test__text_line_copy__whole(&s);
    test__text_line_copy__range(&s);
    test__text_line_resize__shorten(&s);
    test__text_line_resize__lengthen(&s);
    text_buffer_append_f(s.log_buffer, "");

    text_buffer_append_f(s.log_buffer, "TEXT BUFFER TESTS:");
    text_buffer_append_f(s.log_buffer, "");

    text_buffer_append_f(s.log_buffer, "CURSOR POS TESTS:");
    test__cursor_pos_to_start_of_buffer__regular_buffer(&s);
    text_buffer_append_f(s.log_buffer, "");

    _unit_tests_finish(&s);
}
