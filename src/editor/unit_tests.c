#include "unit_tests.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/string_builder.h"
#include "text_buffer.h"

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

bool validate__text_line(Text_Line text_line, const char *str)
{
    int len = strlen(str);
    bool same_str = strcmp(text_line.str, str) == 0;
    bool different_pointers = text_line.str != str;
    bool correct_len = text_line.len == len;
    bool correct_buf_len = text_line.buf_len == len + 1;
    bool null_terminator = text_line.str[text_line.len] == '\0';
    return same_str && different_pointers && correct_len && correct_buf_len && null_terminator;
}

void test__text_line_make_dup(UT_State *s)
{
    char *str = "Hello!!!";
    Text_Line text_line = text_line_make_dup(str);
    bool when_non_empty = validate__text_line(text_line, str);

    char *str_empty = "";
    Text_Line text_line_empty = text_line_make_dup(str_empty);
    bool when_empty = validate__text_line(text_line_empty, str_empty);

    UNIT_TESTS_RUN_CHECK(when_non_empty && when_empty);

    free(text_line.str);
    free(text_line_empty.str);
}

void test__text_line_make_f(UT_State *s)
{
    Text_Line text_line = text_line_make_f("%s %d %.1f", "Hello", 123, 0.3f);
    bool correct = validate__text_line(text_line, "Hello 123 0.3\n");

    UNIT_TESTS_RUN_CHECK(correct);

    free(text_line.str);
}

void test__text_line_copy(UT_State *s)
{
    Text_Line original = text_line_make_dup("Hello 123 0.3\n");

    Text_Line copied_whole = text_line_copy(original, 0, -1);
    bool when_whole = validate__text_line(copied_whole, "Hello 123 0.3\n");

    Text_Line copied_range = text_line_copy(original, 6, 9);
    bool when_range = validate__text_line(copied_range, "123");

    UNIT_TESTS_RUN_CHECK(when_whole && when_range);

    free(original.str);
    free(copied_whole.str);
    free(copied_range.str);
}

void test__text_line_insert_char(UT_State *s)
{
    Text_Line text_line = text_line_make_dup("01234");

    text_line_insert_char(&text_line, 'a', 0);
    bool in_the_beginning = validate__text_line(text_line, "a01234");

    text_line_insert_char(&text_line, 'b', 4);
    bool in_the_middle = validate__text_line(text_line, "a012b34");

    text_line_insert_char(&text_line, 'c', 7);
    bool in_the_end = validate__text_line(text_line, "a012b34c");

    Text_Line text_line_empty = text_line_make_dup("");
    text_line_insert_char(&text_line_empty, '0', 0);
    bool when_empty = validate__text_line(text_line_empty, "0");

    UNIT_TESTS_RUN_CHECK(in_the_beginning && in_the_middle && in_the_end && when_empty);

    free(text_line.str);
    free(text_line_empty.str);
}

void test__text_line_remove_char(UT_State *s)
{
    Text_Line text_line = text_line_make_dup("01234");

    text_line_remove_char(&text_line, 0);
    bool in_the_beginning = validate__text_line(text_line, "1234");

    text_line_remove_char(&text_line, 2);
    bool in_the_middle = validate__text_line(text_line, "124");

    text_line_remove_char(&text_line, 2);
    bool in_the_end = validate__text_line(text_line, "12");

    Text_Line text_line_one_char = text_line_make_dup("0");
    text_line_remove_char(&text_line_one_char, 0);
    bool when_one_char = validate__text_line(text_line_one_char, "");

    UNIT_TESTS_RUN_CHECK(in_the_beginning && in_the_middle && in_the_end && when_one_char);

    free(text_line.str);
    free(text_line_one_char.str);
}

void test__text_line_insert_range(UT_State *s)
{
    Text_Line text_line_a = text_line_make_dup("01234");
    text_line_insert_range(&text_line_a, "ab", 0, 2);
    bool in_the_beginning = validate__text_line(text_line_a, "ab01234");

    Text_Line text_line_b = text_line_make_dup("01234");
    text_line_insert_range(&text_line_b, "ab", 2, 2);
    bool in_the_middle = validate__text_line(text_line_b, "01ab234");

    Text_Line text_line_c = text_line_make_dup("01234");
    text_line_insert_range(&text_line_c, "ab", 5, 2);
    bool in_the_end = validate__text_line(text_line_c, "01234ab");

    Text_Line text_line_d = text_line_make_dup("");
    text_line_insert_range(&text_line_d, "ab", 0, 2);
    bool when_empty = validate__text_line(text_line_d, "ab");

    Text_Line text_line_e = text_line_make_dup("01234");
    text_line_insert_range(&text_line_e, "abcdef", 0, 2);
    bool insert_partial = validate__text_line(text_line_e, "ab01234");

    UNIT_TESTS_RUN_CHECK(in_the_beginning && in_the_middle && in_the_end && when_empty && insert_partial);

    free(text_line_a.str);
    free(text_line_b.str);
    free(text_line_c.str);
    free(text_line_d.str);
    free(text_line_e.str);
}

void test__text_line_remove_range(UT_State *s)
{
    Text_Line text_line_a = text_line_make_dup("01234");
    text_line_remove_range(&text_line_a, 0, 2);
    bool in_the_beginning = validate__text_line(text_line_a, "234");

    Text_Line text_line_b = text_line_make_dup("01234");
    text_line_remove_range(&text_line_b, 2, 2);
    bool in_the_middle = validate__text_line(text_line_b, "014");

    Text_Line text_line_c = text_line_make_dup("01234");
    text_line_remove_range(&text_line_c, 3, 2);
    bool in_the_end = validate__text_line(text_line_c, "012");

    Text_Line text_line_d = text_line_make_dup("01234");
    text_line_remove_range(&text_line_d, 0, 5);
    bool the_whole_string = validate__text_line(text_line_d, "");

    UNIT_TESTS_RUN_CHECK(in_the_beginning && in_the_middle && in_the_end && the_whole_string);

    free (text_line_a.str);
    free (text_line_b.str);
    free (text_line_c.str);
    free (text_line_d.str);
}

bool validate__text_buffer(Text_Buffer *text_buffer, ...)
{
    va_list args;
    va_start(args, text_buffer);
    const char *test_line = va_arg(args, const char *);
    int line_count = 0;
    while (test_line)
    {
        line_count++;
        test_line = va_arg(args, const char *);
    }
    va_end(args);

    if (line_count != text_buffer->line_count)
    {
        return false;
    }

    va_start(args, text_buffer);
    for (int i = 0; i < line_count; i++)
    {
        const char *test_line = va_arg(args, const char *);
        if (!validate__text_line(text_buffer->lines[i], test_line))
        {
            va_end(args);
            return false;
        }
    }
    va_end(args);
    return true;
}

void test__text_buffer_create_from_lines__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        "",
        "123456",
        NULL);

    int line_count = 4;

    bool correct_line_count = text_buffer.line_count == line_count;
    bool correct_strs =
        strcmp(text_buffer.lines[0].str, "Hello, world!!!\n") == 0 &&
        strcmp(text_buffer.lines[1].str, "Test 123\n") == 0 &&
        strcmp(text_buffer.lines[2].str, "\n") == 0 &&
        strcmp(text_buffer.lines[3].str, "123456\n") == 0;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_strs);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_append_line__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        NULL);
    char *line0_old = text_buffer.lines[0].str;
    char *line1_old = text_buffer.lines[1].str;

    Text_Line new_line = text_line_make_dup("New line\n");
    text_buffer_append_line(&text_buffer, new_line);

    bool correct_line_count = text_buffer.line_count == 3;
    bool correct_line0 = text_buffer.lines[0].str == line0_old;
    bool correct_line1 = text_buffer.lines[1].str == line1_old;
    bool correct_line2 = text_buffer.lines[2].str == new_line.str;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0 && correct_line1 && correct_line2);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_append_line__empty(UT_State *s)
{
    Text_Buffer text_buffer = {0};

    Text_Line new_line = text_line_make_dup("New line\n");
    text_buffer_append_line(&text_buffer, new_line);

    bool correct_line_count = text_buffer.line_count == 1;
    bool correct_line0 = text_buffer.lines[0].str == new_line.str;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_insert_line__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        NULL);
    char *line0_old = text_buffer.lines[0].str;
    char *line1_old = text_buffer.lines[1].str;

    Text_Line new_line = text_line_make_dup("New line\n");
    text_buffer_insert_line(&text_buffer, new_line, 1);

    bool correct_line_count = text_buffer.line_count == 3;
    bool same_line0 = text_buffer.lines[0].str == line0_old;
    bool correct_line1 = text_buffer.lines[1].str == new_line.str;
    bool correct_line2 = text_buffer.lines[2].str == line1_old;

    UNIT_TESTS_RUN_CHECK(correct_line_count && same_line0 && correct_line1 && correct_line2);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_insert_line__at_start(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        NULL);
    char *line0_old = text_buffer.lines[0].str;
    char *line1_old = text_buffer.lines[1].str;

    Text_Line new_line = text_line_make_dup("New line\n");
    text_buffer_insert_line(&text_buffer, new_line, 0);

    bool correct_line_count = text_buffer.line_count == 3;
    bool correct_line0 = text_buffer.lines[0].str == new_line.str;
    bool correct_line1 = text_buffer.lines[1].str == line0_old;
    bool correct_line2 = text_buffer.lines[2].str == line1_old;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0 && correct_line1 && correct_line2);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_insert_line__at_end(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        NULL);
    char *line0_old = text_buffer.lines[0].str;
    char *line1_old = text_buffer.lines[1].str;

    Text_Line new_line = text_line_make_dup("New line\n");
    text_buffer_insert_line(&text_buffer, new_line, 2);

    bool correct_line_count = text_buffer.line_count == 3;
    bool correct_line0 = text_buffer.lines[0].str == line0_old;
    bool correct_line1 = text_buffer.lines[1].str == line1_old;
    bool correct_line2 = text_buffer.lines[2].str == new_line.str;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0 && correct_line1 && correct_line2);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_insert_line__empty(UT_State *s)
{
    Text_Buffer text_buffer = {0};

    Text_Line new_line = text_line_make_dup("New line\n");
    text_buffer_insert_line(&text_buffer, new_line, 0);

    bool correct_line_count = text_buffer.line_count == 1;
    bool correct_line0 = text_buffer.lines[0].str == new_line.str;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_remove_line__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        "TEST TEST",
        NULL);
    char *line0_old = text_buffer.lines[0].str;
    char *line2_old = text_buffer.lines[2].str;

    text_buffer_remove_line(&text_buffer, 1);

    bool correct_line_count = text_buffer.line_count == 2;
    bool correct_line0 = text_buffer.lines[0].str == line0_old;
    bool correct_line1 = text_buffer.lines[1].str == line2_old;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0 && correct_line1);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_remove_line__at_start(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        "TEST TEST",
        NULL);
    char *line1_old = text_buffer.lines[1].str;
    char *line2_old = text_buffer.lines[2].str;

    text_buffer_remove_line(&text_buffer, 0);

    bool correct_line_count = text_buffer.line_count == 2;
    bool correct_line0 = text_buffer.lines[0].str == line1_old;
    bool correct_line1 = text_buffer.lines[1].str == line2_old;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0 && correct_line1);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_remove_line__at_end(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        "Test 123",
        "TEST TEST",
        NULL);
    char *line0_old = text_buffer.lines[0].str;
    char *line1_old = text_buffer.lines[1].str;

    text_buffer_remove_line(&text_buffer, 2);

    bool correct_line_count = text_buffer.line_count == 2;
    bool correct_line0 = text_buffer.lines[0].str == line0_old;
    bool correct_line1 = text_buffer.lines[1].str == line1_old;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0 && correct_line1);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_remove_line__only_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello, world!!!",
        NULL);

    text_buffer_remove_line(&text_buffer, 0);

    bool correct_line_count = text_buffer.line_count == 1;
    bool correct_line0 = strcmp(text_buffer.lines[0].str, "\n") == 0;

    UNIT_TESTS_RUN_CHECK(correct_line_count && correct_line0);

    text_buffer_destroy(&text_buffer);
}

void test__text_buffer_insert_char(UT_State *s)
{
    Text_Buffer text_buffer_a = text_buffer_create_from_lines(
        "Line one",
        "Line two",
        NULL);
    text_buffer_insert_char(&text_buffer_a, 'a', (Cursor_Pos){0, 1});
    bool regular_char = validate__text_buffer(&text_buffer_a, 
        "Laine one\n",
        "Line two\n",
        NULL);

    Text_Buffer text_buffer_b = text_buffer_create_from_lines(
        "Line one",
        "Line two",
        NULL);
    text_buffer_insert_char(&text_buffer_b, '\n', (Cursor_Pos){0, 4});
    bool new_line_char = validate__text_buffer(&text_buffer_b,
        "Line\n",
        " one\n",
        "Line two\n",
        NULL);

    Text_Buffer text_buffer_c = text_buffer_create_from_lines(
        "",
        NULL);
    text_buffer_insert_char(&text_buffer_c, 'a', (Cursor_Pos){0, 0});
    bool regular_char_when_empty = validate__text_buffer(&text_buffer_c,
        "a\n",
        NULL);

    Text_Buffer text_buffer_d = text_buffer_create_from_lines(
        "",
        NULL);
    text_buffer_insert_char(&text_buffer_d, '\n', (Cursor_Pos){0, 0});
    bool new_line_char_when_empty = validate__text_buffer(&text_buffer_d,
        "\n",
        "\n",
        NULL);

    UNIT_TESTS_RUN_CHECK(regular_char && new_line_char && regular_char_when_empty && new_line_char_when_empty);

    text_buffer_destroy(&text_buffer_a);
    text_buffer_destroy(&text_buffer_b);
    text_buffer_destroy(&text_buffer_c);
    text_buffer_destroy(&text_buffer_d);
}

void test__text_buffer_remove_char(UT_State *s)
{
    Text_Buffer text_buffer_a = text_buffer_create_from_lines(
        "Line one",
        "Line two",
        NULL);
    text_buffer_remove_char(&text_buffer_a, (Cursor_Pos){0, 2});
    bool regular_char = validate__text_buffer(&text_buffer_a,
        "Lie one\n",
        "Line two\n",
        NULL);

    Text_Buffer text_buffer_b = text_buffer_create_from_lines(
        "Line one",
        "Line two",
        NULL);
    text_buffer_remove_char(&text_buffer_b, (Cursor_Pos){0, 8});
    bool newline_char = validate__text_buffer(&text_buffer_b,
        "Line oneLine two\n",
        NULL);

    Text_Buffer text_buffer_c = text_buffer_create_from_lines(
        "",
        NULL);
    text_buffer_remove_char(&text_buffer_c, (Cursor_Pos){0, 0});
    bool when_removing_the_only_newline_char = validate__text_buffer(&text_buffer_c,
        "\n",
        NULL);

    UNIT_TESTS_RUN_CHECK(regular_char && newline_char && when_removing_the_only_newline_char);

    text_buffer_destroy(&text_buffer_a);
    text_buffer_destroy(&text_buffer_b);
    text_buffer_destroy(&text_buffer_c);
}

void test__text_buffer_insert_range(UT_State *s)
{
    Text_Buffer text_buffer_a = text_buffer_create_from_lines(
        "abcd",
        NULL);
    text_buffer_insert_range(&text_buffer_a, "01\n23\n45", (Cursor_Pos){0, 2});
    bool insert_three_lines = validate__text_buffer(&text_buffer_a,
        "ab01\n",
        "23\n",
        "45cd\n",
        NULL);

    Text_Buffer text_buffer_b = text_buffer_create_from_lines(
        "abcd",
        NULL);
    text_buffer_insert_range(&text_buffer_b, "\n23", (Cursor_Pos){0, 2});
    bool insert_start_on_newline = validate__text_buffer(&text_buffer_b,
        "ab\n",
        "23cd\n",
        NULL);

    Text_Buffer text_buffer_c = text_buffer_create_from_lines(
        "abcd",
        NULL);
    text_buffer_insert_range(&text_buffer_c, "23\n", (Cursor_Pos){0, 2});
    bool insert_end_on_newline = validate__text_buffer(&text_buffer_c,
        "ab23\n",
        "cd\n",
        NULL);

    Text_Buffer text_buffer_d = text_buffer_create_from_lines(
        "abcd",
        NULL);
    text_buffer_insert_range(&text_buffer_d, "\n\n", (Cursor_Pos){0, 2});
    bool insert_two_newlines = validate__text_buffer(&text_buffer_d,
        "ab\n",
        "\n",
        "cd\n",
        NULL);

    Text_Buffer text_buffer_e = text_buffer_create_from_lines(
        "abcd",
        NULL);
    text_buffer_insert_range(&text_buffer_e, "\n", (Cursor_Pos){0, 2});
    bool insert_newline_char = validate__text_buffer(&text_buffer_e,
        "ab\n",
        "cd\n",
        NULL);

    Text_Buffer text_buffer_f = text_buffer_create_from_lines(
        "abcd",
        NULL);
    text_buffer_insert_range(&text_buffer_f, "0123", (Cursor_Pos){0, 2});
    bool insert_no_newlines = validate__text_buffer(&text_buffer_f,
        "ab0123cd\n",
        NULL);

    Text_Buffer text_buffer_g = text_buffer_create_from_lines(
        "",
        NULL);
    text_buffer_insert_range(&text_buffer_g, "\n\n", (Cursor_Pos){0, 0});
    bool insert_when_empty = validate__text_buffer(&text_buffer_g,
        "\n",
        "\n",
        "\n",
        NULL);

    Text_Buffer text_buffer_h = text_buffer_create_from_lines(
        "abcd",
        NULL);
    Cursor_Pos end_cursor = text_buffer_insert_range(&text_buffer_h, "01\n23\n45", (Cursor_Pos){0, 4});
    bool insert_at_the_end = validate__text_buffer(&text_buffer_h,
        "abcd01\n",
        "23\n",
        "45\n",
        NULL);
    bool end_cursor_correct = end_cursor.line == 2 && end_cursor.col == 2;

    UNIT_TESTS_RUN_CHECK(insert_three_lines && insert_start_on_newline && insert_end_on_newline && insert_two_newlines && insert_newline_char && insert_no_newlines && insert_when_empty && insert_at_the_end && end_cursor_correct);

    text_buffer_destroy(&text_buffer_a);
    text_buffer_destroy(&text_buffer_b);
    text_buffer_destroy(&text_buffer_c);
    text_buffer_destroy(&text_buffer_d);
    text_buffer_destroy(&text_buffer_e);
    text_buffer_destroy(&text_buffer_f);
    text_buffer_destroy(&text_buffer_g);
    text_buffer_destroy(&text_buffer_h);
}

void test__text_buffer_remove_range(UT_State *s)
{
    Text_Buffer text_buffer_a = text_buffer_create_from_lines(
        "abcd",
        "efgh",
        "ijkl",
        NULL);
    text_buffer_remove_range(&text_buffer_a, (Cursor_Pos){0, 2}, (Cursor_Pos){2, 2});
    bool remove_across_three_linees = validate__text_buffer(&text_buffer_a,
        "abkl\n",
        NULL);

    Text_Buffer text_buffer_b = text_buffer_create_from_lines(
        "abcd",
        "efgh",
        "ijkl",
        NULL);
    text_buffer_remove_range(&text_buffer_b, (Cursor_Pos){1, 0}, (Cursor_Pos){1, 4});
    bool remove_range_in_line = validate__text_buffer(&text_buffer_b,
        "abcd\n",
        "\n",
        "ijkl\n",
        NULL);

    Text_Buffer text_buffer_c = text_buffer_create_from_lines(
        "abcd",
        "efgh",
        "ijkl",
        NULL);
    text_buffer_remove_range(&text_buffer_c, (Cursor_Pos){1, 4}, (Cursor_Pos){2, 0});
    bool join_two_lines = validate__text_buffer(&text_buffer_c,
        "abcd\n",
        "efghijkl\n",
        NULL);

    Text_Buffer text_buffer_d = text_buffer_create_from_lines(
        "abcd",
        "efgh",
        "ijkl",
        NULL);
    text_buffer_remove_range(&text_buffer_d, (Cursor_Pos){0, 0}, (Cursor_Pos){2, 4});
    bool remove_everything = validate__text_buffer(&text_buffer_d,
        "\n",
        NULL);

    UNIT_TESTS_RUN_CHECK(remove_across_three_linees && remove_range_in_line && join_two_lines && remove_everything);

    text_buffer_destroy(&text_buffer_a);
    text_buffer_destroy(&text_buffer_b);
    text_buffer_destroy(&text_buffer_c);
    text_buffer_destroy(&text_buffer_d);
}

void test__text_buffer_extract_range(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "line 1",
        "line 2",
        "line 3",
        "line 4",
        "line 5",
        NULL);

    char *extracted_range_multi_line = text_buffer_extract_range(&text_buffer, (Cursor_Pos){1, 2}, (Cursor_Pos){3, 3});
    char *extracted_range_partial_line = text_buffer_extract_range(&text_buffer, (Cursor_Pos){1, 2}, (Cursor_Pos){1, 6});
    char *extracted_range_whole = text_buffer_extract_range(&text_buffer, (Cursor_Pos){0, 0}, (Cursor_Pos){4, 7});

    bool multi_line = strcmp(extracted_range_multi_line, "ne 2\nline 3\nlin") == 0;
    bool partial_line = strcmp(extracted_range_partial_line, "ne 2") == 0;
    bool whole = strcmp(extracted_range_whole, "line 1\nline 2\nline 3\nline 4\nline 5\n") == 0;

    UNIT_TESTS_RUN_CHECK(multi_line && partial_line && whole);

    free(extracted_range_multi_line);
    free(extracted_range_partial_line);
    free(extracted_range_whole);
    text_buffer_destroy(&text_buffer);
}

// void test__text_buffer_whitespace_cleanup(UT_State *s)
// {
//     Text_Buffer text_buffer = text_buffer_create_from_lines(
//         "line 1   ",
//         "line 2 ",
//         "line 3",
//         "line 4        ",
//         "line 5 asd",
//         NULL);

//     int cleaned_lines = text_buffer_whitespace_cleanup(&text_buffer);
//     bool correct_buffer = validate__text_buffer(&text_buffer,
//         "line 1\n",
//         "line 2\n",
//         "line 3\n",
//         "line 4\n",
//         "line 5 asd\n",
//         NULL);
//     bool correct_count = cleaned_lines == 3;

//     UNIT_TESTS_RUN_CHECK(correct_buffer && correct_count);

//     text_buffer_destroy(&text_buffer);
// }

void test__cursor_pos_clamp__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 3};

    Cursor_Pos new_pos = cursor_pos_clamp(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == pos.line && new_pos.col == pos.col);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_clamp__col_past_end_of_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 20};

    Cursor_Pos new_pos = cursor_pos_clamp(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == pos.line && new_pos.col == 15);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_clamp__col_neg(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, -2};

    Cursor_Pos new_pos = cursor_pos_clamp(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == pos.line && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_clamp__line_neg(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {-1, 30};

    Cursor_Pos new_pos = cursor_pos_clamp(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_clamp__line_past_end_of_buffer(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {2, 30};

    Cursor_Pos new_pos = cursor_pos_clamp(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 15);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_char__forward_regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 2};

    Cursor_Pos new_pos = cursor_pos_advance_char(text_buffer, pos, +1, true);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 3);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_char__backward_regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 2};

    Cursor_Pos new_pos = cursor_pos_advance_char(text_buffer, pos, -1, true);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 1);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_char__forward_switch_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 8};

    Cursor_Pos new_pos = cursor_pos_advance_char(text_buffer, pos, +1, true);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_char__backward_switch_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 0};

    Cursor_Pos new_pos = cursor_pos_advance_char(text_buffer, pos, -1, true);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_char__forward_clamp(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 8};

    Cursor_Pos new_pos = cursor_pos_advance_char(text_buffer, pos, +1, false);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_char__backward_clamp(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 0};

    Cursor_Pos new_pos = cursor_pos_advance_char(text_buffer, pos, -1, false);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_line__forward_regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 4};

    Cursor_Pos new_pos = cursor_pos_advance_line(text_buffer, pos, +1);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_line__backward_regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 4};

    Cursor_Pos new_pos = cursor_pos_advance_line(text_buffer, pos, -1);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_line__forward_clamp_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "And good bye!!!",
        "Hello!!!",
        NULL);
    Cursor_Pos pos = {0, 11};

    Cursor_Pos new_pos = cursor_pos_advance_line(text_buffer, pos, +1);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_line__backward_clamp_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 11};

    Cursor_Pos new_pos = cursor_pos_advance_line(text_buffer, pos, -1);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_line__forward_last_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 6};

    Cursor_Pos new_pos = cursor_pos_advance_line(text_buffer, pos, +1);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 15);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_advance_line__backward_first_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 6};

    Cursor_Pos new_pos = cursor_pos_advance_line(text_buffer, pos, -1);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_start_of_buffer__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 6};

    Cursor_Pos new_pos = cursor_pos_to_start_of_buffer(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_end_of_buffer__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 6};

    Cursor_Pos new_pos = cursor_pos_to_end_of_buffer(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 15);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_start_of_line__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {1, 6};

    Cursor_Pos new_pos = cursor_pos_to_start_of_line(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_end_of_line__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "Hello!!!",
        "And good bye!!!",
        NULL);
    Cursor_Pos pos = {0, 2};

    Cursor_Pos new_pos = cursor_pos_to_end_of_line(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_word__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 0};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_word__start_at_space(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 3};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_word__start_at_new_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 13};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_word__skip_current(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 4};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_word__next_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 8};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_word__last_word(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {1, 10};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 1 && new_pos.col == 13);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_word__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 5};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_word__start_at_space(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 7};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_word__skip_current(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 8};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_word__prev_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {1, 0};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 8);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_word__first_word(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One Two Three",
        "Four Five Six",
        NULL);
    Cursor_Pos pos = {0, 1};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_word(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_paragraph__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One",
        "Two",
        "",
        "",
        "Three",
        "Four",
        NULL);
    Cursor_Pos pos = {1, 3};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 4 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_paragraph__skip_current(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One",
        "Two",
        "",
        "",
        "Three",
        "Four",
        NULL);
    Cursor_Pos pos = {0, 0};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 4 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_paragraph__last_paragraph(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "OneOne",
        "Two",
        "",
        "",
        "Three",
        "Four",
        NULL);
    Cursor_Pos pos = {4, 1};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 5 && new_pos.col == 4);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_paragraph__last_white_lines(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "OneOne",
        "Two",
        "",
        "",
        "Three",
        "Four",
        "",
        "",
        NULL);
    Cursor_Pos pos = {4, 1};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 7 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_paragraph__start_on_white_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "OneOne",
        "Two",
        "",
        "",
        "Three",
        "Four",
        "",
        "",
        NULL);
    Cursor_Pos pos = {3, 0};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 4 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_next_start_of_paragraph__start_on_last_white_line(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "OneOne",
        "Two",
        "",
        "",
        "Three",
        "Four",
        "",
        "",
        NULL);
    Cursor_Pos pos = {6, 0};

    Cursor_Pos new_pos = cursor_pos_to_next_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 7 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_paragraph__regular(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One",
        "Two",
        "",
        "",
        "Three",
        "Four",
        NULL);
    Cursor_Pos pos = {4, 1};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_paragraph__skip_current(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "One",
        "Two",
        "",
        "",
        "Three",
        "Four",
        NULL);
    Cursor_Pos pos = {4, 0};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__cursor_pos_to_prev_start_of_paragraph__start_at_first_white_lines(UT_State *s)
{
    Text_Buffer text_buffer = text_buffer_create_from_lines(
        "",
        "",
        "One",
        "Two",
        "",
        "",
        "Three",
        "Four",
        NULL);
    Cursor_Pos pos = {1, 0};

    Cursor_Pos new_pos = cursor_pos_to_prev_start_of_paragraph(text_buffer, pos);

    UNIT_TESTS_RUN_CHECK(new_pos.line == 0 && new_pos.col == 0);

    text_buffer_destroy(&text_buffer);
}

void test__string_builder(UT_State *s)
{
    String_Builder sb = {0};
    string_builder_append_f(&sb, "Hello %s\n", "world!");
    string_builder_append_f(&sb, "%d %d %d", 1, 2, 3);
    string_builder_append_f(&sb, "\n\n\n");
    string_builder_append_str_range(&sb, "012345", 0, 6);
    string_builder_append_str_range(&sb, "012345", 1, 3);

    bool correct_chunk_count = sb.chunk_count == 5;
    bool correct_chunks = strcmp(sb.chunks[0], "Hello world!\n") == 0 &&
                          strcmp(sb.chunks[1], "1 2 3") == 0 &&
                          strcmp(sb.chunks[2], "\n\n\n") == 0 &&
                          strcmp(sb.chunks[3], "012345") == 0 &&
                          strcmp(sb.chunks[4], "123") == 0;

    char *compiled_str = string_builder_compile_and_destroy(&sb);
    bool correct_compiled = strcmp(compiled_str, "Hello world!\n1 2 3\n\n\n012345123") == 0;
    bool string_builder_destroyed = sb.chunks == NULL && sb.chunk_count == 0;

    UNIT_TESTS_RUN_CHECK(correct_chunk_count && correct_chunks && correct_compiled && string_builder_destroyed);

    free(compiled_str);
}

// ---------------------------------------------------------------------

void unit_tests_run(Text_Buffer *log_buffer, bool break_on_failure)
{
    UT_State s = {0};
    s.log_buffer = log_buffer;
    s.break_on_failure = break_on_failure;

    text_buffer_append_f(s.log_buffer, "TEXT LINE TESTS:");
    test__text_line_make_dup(&s);
    test__text_line_make_f(&s);
    test__text_line_copy(&s);
    test__text_line_insert_char(&s);
    test__text_line_remove_char(&s);
    test__text_line_insert_range(&s);
    test__text_line_remove_range(&s);
    text_buffer_append_f(s.log_buffer, "");

    text_buffer_append_f(s.log_buffer, "TEXT BUFFER TESTS:");
    test__text_buffer_create_from_lines__regular(&s);
    test__text_buffer_append_line__regular(&s);
    test__text_buffer_append_line__empty(&s);
    test__text_buffer_insert_line__regular(&s);
    test__text_buffer_insert_line__at_start(&s);
    test__text_buffer_insert_line__at_end(&s);
    test__text_buffer_insert_line__empty(&s);
    test__text_buffer_remove_line__regular(&s);
    test__text_buffer_remove_line__at_start(&s);
    test__text_buffer_remove_line__at_end(&s);
    test__text_buffer_remove_line__only_line(&s);
    test__text_buffer_insert_char(&s);
    test__text_buffer_remove_char(&s);
    test__text_buffer_insert_range(&s);
    test__text_buffer_remove_range(&s);
    test__text_buffer_extract_range(&s);
    text_buffer_append_f(s.log_buffer, "");

    text_buffer_append_f(s.log_buffer, "CURSOR POS TESTS:");
    test__cursor_pos_clamp__regular(&s);
    test__cursor_pos_clamp__col_past_end_of_line(&s);
    test__cursor_pos_clamp__col_neg(&s);
    test__cursor_pos_clamp__line_neg(&s);
    test__cursor_pos_advance_char__forward_regular(&s);
    test__cursor_pos_advance_char__backward_regular(&s);
    test__cursor_pos_advance_char__forward_switch_line(&s);
    test__cursor_pos_advance_char__backward_switch_line(&s);
    test__cursor_pos_advance_char__forward_clamp(&s);
    test__cursor_pos_advance_char__backward_clamp(&s);
    test__cursor_pos_advance_line__forward_regular(&s);
    test__cursor_pos_advance_line__backward_regular(&s);
    test__cursor_pos_advance_line__forward_clamp_line(&s);
    test__cursor_pos_advance_line__backward_clamp_line(&s);
    test__cursor_pos_advance_line__forward_last_line(&s);
    test__cursor_pos_advance_line__backward_first_line(&s);
    test__cursor_pos_to_start_of_buffer__regular(&s);
    test__cursor_pos_to_end_of_buffer__regular(&s);
    test__cursor_pos_to_start_of_line__regular(&s);
    test__cursor_pos_to_end_of_line__regular(&s);
    test__cursor_pos_to_next_start_of_word__regular(&s);
    test__cursor_pos_to_next_start_of_word__start_at_space(&s);
    test__cursor_pos_to_next_start_of_word__start_at_new_line(&s);
    test__cursor_pos_to_next_start_of_word__skip_current(&s);
    test__cursor_pos_to_next_start_of_word__next_line(&s);
    test__cursor_pos_to_next_start_of_word__last_word(&s);
    test__cursor_pos_to_prev_start_of_word__regular(&s);
    test__cursor_pos_to_prev_start_of_word__start_at_space(&s);
    test__cursor_pos_to_prev_start_of_word__skip_current(&s);
    test__cursor_pos_to_prev_start_of_word__prev_line(&s);
    test__cursor_pos_to_prev_start_of_word__first_word(&s);
    test__cursor_pos_to_next_start_of_paragraph__regular(&s);
    test__cursor_pos_to_next_start_of_paragraph__skip_current(&s);
    test__cursor_pos_to_next_start_of_paragraph__last_paragraph(&s);
    test__cursor_pos_to_next_start_of_paragraph__last_white_lines(&s);
    test__cursor_pos_to_next_start_of_paragraph__start_on_white_line(&s);
    test__cursor_pos_to_next_start_of_paragraph__start_on_last_white_line(&s);
    test__cursor_pos_to_prev_start_of_paragraph__regular(&s);
    test__cursor_pos_to_prev_start_of_paragraph__skip_current(&s);
    test__cursor_pos_to_prev_start_of_paragraph__start_at_first_white_lines(&s);
    text_buffer_append_f(s.log_buffer, "");

    text_buffer_append_f(s.log_buffer, "STRING BUILDER TESTS:");
    test__string_builder(&s);
    text_buffer_append_f(s.log_buffer, "");

    _unit_tests_finish(&s);
}
