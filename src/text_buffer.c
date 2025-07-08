#include "text_buffer.h"

#include <stdbool.h>
#include <ctype.h>

#include "string_builder.h"
#include "util.h"

bool cursor_pos_eq(Cursor_Pos a, Cursor_Pos b)
{
    bool equal = a.line == b.line && a.col == b.col;
    return equal;
}

Cursor_Pos cursor_pos_min(Cursor_Pos a, Cursor_Pos b)
{
    if (a.line < b.line || (a.line == b.line && a.col <= b.col)) return a;
    else return b;
}

Cursor_Pos cursor_pos_max(Cursor_Pos a, Cursor_Pos b)
{
    if (a.line > b.line || (a.line == b.line && a.col >= b.col)) return a;
    else return b;
}

Cursor_Pos cursor_pos_clamp(Text_Buffer text_buffer, Cursor_Pos pos)
{
    if (pos.line < 0)
    {
        pos.line = 0;
        pos.col = 0;
        return pos;
    }
    if (pos.line > text_buffer.line_count - 1)
    {
        pos.line = text_buffer.line_count - 1;
        pos.col = text_buffer.lines[pos.line].len - 1;
        return pos;
    }
    if (pos.col < 0)
    {
        pos.col = 0;
        return pos;
    }
    if (pos.col > text_buffer.lines[pos.line].len - 1)
    {
        pos.col = text_buffer.lines[pos.line].len - 1;
        return pos;
    }
    return pos;
}

Cursor_Pos cursor_pos_advance_char(Text_Buffer text_buffer, Cursor_Pos pos, int dir, bool can_switch_lines)
{
    int line = pos.line;
    int col = pos.col;
    if (dir > 0)
    {
        col++;
        if (col > text_buffer.lines[line].len - 1)
        {
            if (can_switch_lines && line < text_buffer.line_count - 1)
            {
                line++;
                col = 0;
            }
            else
            {
                col = text_buffer.lines[line].len - 1;
            }
        }
    }
    else
    {
        col--;
        if (col < 0)
        {
            if (can_switch_lines && line > 0)
            {
                line--;
                col = text_buffer.lines[line].len - 1;
            }
            else
            {
                col = 0;
            }
        }
    }
    return (Cursor_Pos){line, col};
}

Cursor_Pos cursor_pos_advance_char_n(Text_Buffer text_buffer, Cursor_Pos pos, int n, int dir, bool can_switch_lines)
{
    for (int i = 0; i < n; i++)
    {
        pos = cursor_pos_advance_char(text_buffer, pos, dir, can_switch_lines);
    }
    return pos;
}

Cursor_Pos cursor_pos_advance_line(Text_Buffer text_buffer, Cursor_Pos pos, int dir)
{
    int line = pos.line;
    int col = pos.col;
    if (dir > 0)
    {
        if (line < text_buffer.line_count - 1)
        {
            line++;
            if (col > text_buffer.lines[line].len - 1)
            {
                col = text_buffer.lines[line].len - 1;
            }
        }
        else
        {
            col = text_buffer.lines[line].len - 1;
        }
    }
    else
    {
        if (line > 0)
        {
            line--;
            if (col > text_buffer.lines[line].len - 1)
            {
                col = text_buffer.lines[line].len - 1;
            }
        }
        else
        {
            col = 0;
        }
    }
    return (Cursor_Pos){line, col};
}

Cursor_Pos cursor_pos_to_start_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos)
{
    (void)text_buffer; (void)cursor_pos;
    Cursor_Pos pos = {0};
    return pos;
}

Cursor_Pos cursor_pos_to_end_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos)
{
    (void)cursor_pos;
    Cursor_Pos pos;
    pos.line = text_buffer.line_count - 1;
    pos.col = text_buffer.lines[pos.line].len - 1;
    return pos;
}

Cursor_Pos cursor_pos_to_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos)
{
    (void)text_buffer;
    Cursor_Pos new_pos;
    new_pos.line = pos.line;
    new_pos.col = 0;
    return new_pos;
}

Cursor_Pos cursor_pos_to_indent_or_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos)
{
    int col = text_buffer_line_indent_get_level(&text_buffer, pos.line);
    if (col >= pos.col) col = 0;
    Cursor_Pos new_pos;
    new_pos.line = pos.line;
    new_pos.col = col;
    return new_pos;
}

Cursor_Pos cursor_pos_to_end_of_line(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Pos new_pos;
    new_pos.line = pos.line;
    new_pos.col = text_buffer.lines[new_pos.line].len - 1;
    return new_pos;
}

Cursor_Pos cursor_pos_to_next_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Iterator curr_it = { .buf = &text_buffer, .pos = pos };
    Cursor_Iterator prev_it = curr_it;
    while (cursor_iterator_next(&curr_it))
    {
        char curr = cursor_iterator_get_char(curr_it);
        char prev = cursor_iterator_get_char(prev_it);
        if ((isspace(prev) || ispunct(prev)) && isalnum(curr))
        {
            return curr_it.pos;
        }
        prev_it = curr_it;
    }
    return cursor_pos_to_end_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_next_end_of_word(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Iterator curr_it = { .buf = &text_buffer, .pos = pos };
    Cursor_Iterator prev_it = curr_it;
    while (cursor_iterator_next(&curr_it))
    {
        char curr = cursor_iterator_get_char(curr_it);
        char prev = cursor_iterator_get_char(prev_it);
        if (isalnum(prev) && (isspace(curr) || ispunct(curr)))
        {
            return curr_it.pos;
        }
        prev_it = curr_it;
    }
    return cursor_pos_to_end_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_prev_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Iterator curr_it = { .buf = &text_buffer, .pos = pos };
    cursor_iterator_prev(&curr_it);
    Cursor_Iterator prev_it = curr_it;
    while (cursor_iterator_prev(&curr_it))
    {
        char curr = cursor_iterator_get_char(curr_it);
        char prev = cursor_iterator_get_char(prev_it);
        if ((isspace(curr) || ispunct(curr)) && isalnum(prev))
        {
            return prev_it.pos;
        }
        prev_it = curr_it;
    }
    return cursor_pos_to_start_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_next_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos)
{
    for (int line = pos.line + 1; line < text_buffer.line_count; line++)
    {
        if (is_white_line(text_buffer.lines[line - 1].str) && !is_white_line(text_buffer.lines[line].str))
        {
            return (Cursor_Pos){line, 0};
        }
    }
    return cursor_pos_to_end_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_prev_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos)
{
    for (int line = pos.line - 1; line > 0; line--)
    {
        if (is_white_line(text_buffer.lines[line - 1].str) && !is_white_line(text_buffer.lines[line].str))
        {
            return (Cursor_Pos){line, 0};
        }
    }
    return cursor_pos_to_start_of_buffer(text_buffer, pos);
}

// --------------------------------------------------------------------------------------------------------

bool cursor_iterator_next(Cursor_Iterator *it)
{
    int line = it->pos.line;
    int col = it->pos.col;
    col++;
    if (col > it->buf->lines[line].len - 1)
    {
        if (line < it->buf->line_count - 1)
        {
            line++;
            col = 0;
        }
        else
        {
            return false;
        }
    }
    it->pos.line = line;
    it->pos.col = col;
    return true;
}

bool cursor_iterator_prev(Cursor_Iterator *it)
{
    int line = it->pos.line;
    int col = it->pos.col;
    col--;
    if (col < 0)
    {
        if (line > 0)
        {
            line--;
            col = it->buf->lines[line].len - 1;
        }
        else
        {
            return false;
        }
    }
    it->pos.line = line;
    it->pos.col = col;
    return true;
}

char cursor_iterator_get_char(Cursor_Iterator it)
{
    char c = it.buf->lines[it.pos.line].str[it.pos.col];
    return c;
}

// --------------------------------------------------------------------------------------------------------

Text_Line text_line_make_dup(const char *str)
{
    Text_Line r;
    r.str = xstrdup(str);
    r.len = strlen(r.str);
    r.buf_len = r.len + 1;
    return r;
}

Text_Line text_line_make_dup_range(const char *str, int start, int count)
{
    Text_Line r;
    r.str = xstrndup(str + start, count);
    r.len = strlen(r.str);
    r.buf_len = r.len + 1;
    return r;
}

Text_Line text_line_make_va(const char *fmt, va_list args)
{
    // TODO: Don't use MAX_CHARS_PER_LINE and stack buffer here
    char str_buf[MAX_CHARS_PER_LINE];
    int written_length = vsnprintf(str_buf, sizeof(str_buf) - 1, fmt, args);
    if (written_length < 0) written_length = 0;
    if (written_length >= (int)(sizeof(str_buf) - 1)) written_length = sizeof(str_buf) - 2;

    str_buf[written_length] = '\n';
    str_buf[written_length + 1] = '\0';

    Text_Line line = text_line_make_dup(str_buf);
    return line;
}

Text_Line text_line_make_f(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Text_Line text_line = text_line_make_va(fmt, args);
    va_end(args);
    return text_line;
}

Text_Line text_line_copy(Text_Line source, int start, int end)
{
    Text_Line r;
    if (end < 0) end = source.len;
    r.len = end - start;
    r.buf_len = r.len + 1;
    r.str = xmalloc(r.buf_len);
    strncpy(r.str, source.str + start, r.len);
    r.str[r.len] = '\0';
    return r;
}

void text_line_resize(Text_Line *text_line, int new_size)
{
    text_line->str = xrealloc(text_line->str, new_size + 1);
    text_line->buf_len = new_size + 1;
    text_line->len = new_size;
    text_line->str[new_size] = '\0';
}

void text_line_insert_char(Text_Line *text_line, char c, int insert_index)
{
    bassert(insert_index >= 0);
    bassert(insert_index <= text_line->len);
    text_line_resize(text_line, text_line->len + 1);
    for (int i = text_line->len; i > insert_index; i--)
    {
        text_line->str[i] = text_line->str[i - 1];
    }
    text_line->str[insert_index] = c;
}

void text_line_remove_char(Text_Line *text_line, int remove_index)
{
    bassert(remove_index >= 0);
    bassert(remove_index < text_line->len);
    for (int i = remove_index; i < text_line->len; i++)
    {
        text_line->str[i] = text_line->str[i + 1];
    }
    text_line_resize(text_line, text_line->len - 1);
}

void text_line_insert_range(Text_Line *text_line, const char *range, int insert_index, int insert_count)
{
    bassert(insert_index >= 0);
    bassert(insert_index <= text_line->len);
    bassert(insert_count <= (int)strlen(range));
    text_line_resize(text_line, text_line->len + insert_count);
    for (int i = text_line->len; i >= insert_index + insert_count; i--)
    {
        text_line->str[i] = text_line->str[i - insert_count];
    }
    for (int i = 0; i < insert_count; i++)
    {
        text_line->str[insert_index + i] = *range++;
    }
}

void text_line_remove_range(Text_Line *text_line, int remove_index, int remove_count)
{
    bassert(remove_index >= 0);
    bassert(remove_index + remove_count <= text_line->len);
    for (int i = remove_index; i <= text_line->len - remove_count; i++)
    {
        text_line->str[i] = text_line->str[i + remove_count];
    }
    text_line_resize(text_line, text_line->len - remove_count);
}

// --------------------------------------------------------------------------------------------

Text_Buffer text_buffer_create_from_lines(const char *first, ...)
{
    Text_Buffer text_buffer = {0};
    va_list args;
    va_start(args, first);
    const char *line = first;
    while (line)
    {
        text_buffer_append_f(&text_buffer, "%s", line);
        line = va_arg(args, const char *);
    }
    va_end(args);
    return text_buffer;
}

Text_Buffer text_buffer_create_empty()
{
    Text_Buffer text_buffer = {0};
    text_buffer_append_f(&text_buffer, "");
    return text_buffer;
}

void text_buffer_destroy(Text_Buffer *text_buffer)
{
    for (int i = 0; i < text_buffer->line_count; i++)
    {
        free(text_buffer->lines[i].str);
    }
    free(text_buffer->lines);
    text_buffer->lines = NULL;
    text_buffer->line_count = 0;
}

void text_buffer_validate(Text_Buffer *text_buffer)
{
    for (int i = 0; i < text_buffer->line_count; i++) {
        int actual_len = strlen(text_buffer->lines[i].str);
        bassert(actual_len > 0);
        bassert(actual_len == text_buffer->lines[i].len);
        bassert(text_buffer->lines[i].buf_len == text_buffer->lines[i].len + 1);
        bassert(text_buffer->lines[i].str[actual_len] == '\0');
        bassert(text_buffer->lines[i].str[actual_len - 1] == '\n');
    }
}

void text_buffer_append_line(Text_Buffer *text_buffer, Text_Line text_line)
{
    text_buffer->line_count++;
    text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
    text_buffer->lines[text_buffer->line_count - 1] = text_line;
}

void text_buffer_insert_line(Text_Buffer *text_buffer, Text_Line new_line, int insert_at)
{
    text_buffer->lines = xrealloc(text_buffer->lines, (text_buffer->line_count + 1) * sizeof(text_buffer->lines[0]));
    for (int i = text_buffer->line_count - 1; i >= insert_at ; i--) {
        text_buffer->lines[i + 1] = text_buffer->lines[i];
    }
    text_buffer->line_count++;
    text_buffer->lines[insert_at] = new_line;
}

void text_buffer_remove_line(Text_Buffer *text_buffer, int remove_at)
{
    free(text_buffer->lines[remove_at].str);
    for (int i = remove_at + 1; i <= text_buffer->line_count - 1; i++) {
        text_buffer->lines[i - 1] = text_buffer->lines[i];
    }
    text_buffer->line_count--;
    if (text_buffer->line_count <= 0)
    {
        text_buffer->line_count = 1;
        text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
        text_buffer->lines[0] = text_line_make_dup("\n");
    }
    else
    {
        text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
    }
}

void text_buffer_append_f(Text_Buffer *text_buffer, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Text_Line text_line =  text_line_make_va(fmt, args);
    va_end(args);
    text_buffer_append_line(text_buffer, text_line);
}

void text_buffer_split_line(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    Text_Line *current_line = &text_buffer->lines[pos.line];
    int chars_moved_to_next_line = current_line->len - pos.col;
    Text_Line new_line = text_line_make_dup_range(current_line->str, pos.col, chars_moved_to_next_line);
    text_line_remove_range(current_line, pos.col, chars_moved_to_next_line);
    text_buffer_insert_line(text_buffer, new_line, pos.line + 1);
}

void text_buffer_clear(Text_Buffer *text_buffer)
{
    text_buffer_destroy(text_buffer);
    text_buffer_append_f(text_buffer, "");
}

void text_buffer_insert_char(Text_Buffer *text_buffer, char c, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    if (c == '\n')
    {
        text_buffer_split_line(text_buffer, pos);
    }
    text_line_insert_char(&text_buffer->lines[pos.line], c, pos.col);
}

char text_buffer_remove_char(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    bool deleting_line_break = pos.col == text_buffer->lines[pos.line].len - 1; // Valid text buffer will always have \n at len - 1
    char removed_char = text_buffer->lines[pos.line].str[pos.col];
    Text_Line *this_line = &text_buffer->lines[pos.line];
    if (deleting_line_break)
    {
        if (pos.line < text_buffer->line_count - 1)
        {
            text_line_remove_char(this_line, pos.col);
            Text_Line *next_line = &text_buffer->lines[pos.line + 1];
            text_line_insert_range(this_line, next_line->str, this_line->len, next_line->len);
            text_buffer_remove_line(text_buffer, pos.line + 1);
        }
    }
    else
    {
        text_line_remove_char(this_line, pos.col);
    }
    return removed_char;
}

Cursor_Pos text_buffer_insert_range(Text_Buffer *text_buffer, const char *range, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    int range_len = strlen(range);
    bassert(range_len > 0);
    int segment_count = str_get_line_segment_count(range);

    Cursor_Pos end_cursor = pos;
    if (segment_count == 1)
    {
        text_line_insert_range(&text_buffer->lines[pos.line], range, pos.col, range_len);
        end_cursor.col = pos.col + range_len;
    }
    else
    {
        text_buffer_split_line(text_buffer, pos);
        int dest_line_i = pos.line;
        int segment_start = 0;
        for (int i = 0; i < segment_count; i++)
        {
            int segment_end = str_find_next_new_line(range, segment_start);
            if (segment_end != range_len) segment_end++; // If new line, copy the new line char too
            int segment_len = segment_end - segment_start;
            if (i == 0) // first segment
            {
                text_line_insert_range(&text_buffer->lines[dest_line_i], range, pos.col, segment_len);
            }
            else if (i == segment_count - 1) // last segment
            {
                text_line_insert_range(&text_buffer->lines[dest_line_i], range + segment_start, 0, segment_len);
                end_cursor.line = dest_line_i;
                end_cursor.col = segment_len;
            }
            else // middle segments
            {
                Text_Line new_line = text_line_make_dup_range(range, segment_start, segment_len);
                text_buffer_insert_line(text_buffer, new_line, dest_line_i);
            }
            dest_line_i++;
            segment_start = segment_end;
        }
    }
    return end_cursor;
}

void text_buffer_remove_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end)
{
    bassert(start.line < text_buffer->line_count);
    bassert(start.col < text_buffer->lines[start.line].len);
    bassert(end.line < text_buffer->line_count);
    bassert(end.col <= text_buffer->lines[end.line].len);
    bassert((end.line == start.line && end.col > start.col) || end.line > start.line);
    if (start.line == end.line)
    {
        text_line_remove_range(&text_buffer->lines[start.line], start.col, end.col - start.col);
    }
    else
    {
        Text_Line *start_text_line = &text_buffer->lines[start.line];
        Text_Line *end_text_line = &text_buffer->lines[end.line];
        text_line_remove_range(start_text_line, start.col, start_text_line->len - start.col);
        text_line_insert_range(start_text_line, end_text_line->str + end.col, start.col, end_text_line->len - end.col);
        int lines_to_remove = end.line - start.line;
        for (int i = 0; i < lines_to_remove; i++)
        {
            text_buffer_remove_line(text_buffer, start.line + 1); // Keep removing the same index, as lines get shifted up
        }
    }
}

char text_buffer_get_char(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    Text_Line *line = &text_buffer->lines[pos.line];
    bassert(pos.col < line->len);
    return line->str[pos.col];
}

char *text_buffer_extract_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end)
{
    bassert(start.line < text_buffer->line_count);
    bassert(start.col < text_buffer->lines[start.line].len);
    bassert(end.line < text_buffer->line_count);
    bassert(end.col <= text_buffer->lines[end.line].len);
    bassert((end.line == start.line && end.col > start.col) || end.line > start.line);
    String_Builder sb = {0};
    if (start.line == end.line)
    {
        string_builder_append_str_range(&sb,
            text_buffer->lines[start.line].str,
            start.col,
            end.col - start.col);
    }
    else
    {
        string_builder_append_str_range(&sb,
            text_buffer->lines[start.line].str,
            start.col,
            text_buffer->lines[start.line].len - start.col);
        for (int i = start.line + 1; i < end.line; i++)
        {
            string_builder_append_str_range(&sb,
                text_buffer->lines[i].str,
                0,
                text_buffer->lines[i].len);
        }
        string_builder_append_str_range(&sb,
            text_buffer->lines[end.line].str,
            0,
            end.col);
    }
    char *extracted_range = string_builder_compile_and_destroy(&sb);
    return extracted_range;
}

bool text_buffer_search_next(Text_Buffer *text_buffer, const char *query, Cursor_Pos from, Cursor_Pos *out_pos)
{
    int col_offset = from.col + 1;
    for (int i = from.line; i < text_buffer->line_count; i++)
    {
        char *match = strstr(text_buffer->lines[i].str + col_offset, query);
        if (match)
        {
            int col = match - text_buffer->lines[i].str;
            out_pos->line = i;
            out_pos->col = col;
            return true;
        }
        col_offset = 0;
    }
    return false;
}

int text_buffer_line_indent_get_level(Text_Buffer *text_buffer, int line)
{
    int spaces = 0;
    char *str = text_buffer->lines[line].str;
        while (*str)
    {
        if (*str == ' ') spaces++;
        else return spaces;
        str++;
    }
    return 0;
}
