#pragma once

#include <stdarg.h>
#include <stdbool.h>

#define MAX_CHARS_PER_LINE 1024

typedef struct {
    char *str;
    int len;
    int buf_len;
} Text_Line;

typedef struct {
    Text_Line *lines;
    int line_count;
} Text_Buffer;

typedef struct {
    int line;
    int col;
} Cursor_Pos;

typedef struct {
    bool active;
    Cursor_Pos pos;
} Text_Mark;

typedef struct {
    const Text_Buffer *buf;
    Cursor_Pos pos;
} Cursor_Iterator;

bool cursor_pos_eq(Cursor_Pos a, Cursor_Pos b);
Cursor_Pos cursor_pos_min(Cursor_Pos a, Cursor_Pos b);
Cursor_Pos cursor_pos_max(Cursor_Pos a, Cursor_Pos b);
Cursor_Pos cursor_pos_clamp(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_advance_char(Text_Buffer text_buffer, Cursor_Pos pos, int dir, bool can_switch_lines);
Cursor_Pos cursor_pos_advance_char_n(Text_Buffer text_buffer, Cursor_Pos pos, int n, int dir, bool can_switch_lines);
Cursor_Pos cursor_pos_advance_line(Text_Buffer text_buffer, Cursor_Pos pos, int dir);
Cursor_Pos cursor_pos_to_start_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos);
Cursor_Pos cursor_pos_to_end_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos);
Cursor_Pos cursor_pos_to_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_indent_or_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_end_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_end_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_prev_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_prev_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos);

bool cursor_iterator_next(Cursor_Iterator *it);
bool cursor_iterator_prev(Cursor_Iterator *it);
char cursor_iterator_get_char(Cursor_Iterator it);

Text_Line text_line_make_dup(const char *line);
Text_Line text_line_make_dup_range(const char *str, int start, int count);
Text_Line text_line_make_va(const char *fmt, va_list args);
Text_Line text_line_make_f(const char *fmt, ...);
Text_Line text_line_copy(Text_Line source, int start, int end);
void text_line_resize(Text_Line *text_line, int new_size);
void text_line_insert_char(Text_Line *text_line, char c, int insert_index);
void text_line_remove_char(Text_Line *text_line, int remove_index);
void text_line_insert_range(Text_Line *text_line, const char *range, int insert_index, int insert_count);
void text_line_remove_range(Text_Line *text_line, int remove_index, int remove_count);

Text_Buffer text_buffer_create_from_lines(const char *first, ...);
Text_Buffer text_buffer_create_empty();
void text_buffer_destroy(Text_Buffer *text_buffer);
void text_buffer_validate(Text_Buffer *text_buffer);
void text_buffer_append_line(Text_Buffer *text_buffer, Text_Line text_line);
void text_buffer_insert_line(Text_Buffer *text_buffer, Text_Line new_line, int insert_at);
void text_buffer_remove_line(Text_Buffer *text_buffer, int remove_at);
void text_buffer_append_f(Text_Buffer *text_buffer, const char *fmt, ...);
void text_buffer_split_line(Text_Buffer *text_buffer, Cursor_Pos pos);
void text_buffer_clear(Text_Buffer *text_buffer);
void text_buffer_insert_char(Text_Buffer *text_buffer, char c, Cursor_Pos pos);
char text_buffer_remove_char(Text_Buffer *text_buffer, Cursor_Pos pos);
Cursor_Pos text_buffer_insert_range(Text_Buffer *text_buffer, const char *range, Cursor_Pos pos);
void text_buffer_remove_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end);
char text_buffer_get_char(Text_Buffer *text_buffer, Cursor_Pos pos);
char *text_buffer_extract_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end);
bool text_buffer_search_next(Text_Buffer *text_buffer, const char *query, Cursor_Pos from, Cursor_Pos *out_pos);
int text_buffer_line_indent_get_level(Text_Buffer *text_buffer, int line);
