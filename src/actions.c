#include "actions.h"

#include "common.h"

#include <GLFW/glfw3.h>

#include "editor.h"
#include "unit_tests.h"
#include "util.h"

bool action_run_unit_tests(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    Text_Buffer log_buffer = {0};
    unit_tests_run(&log_buffer, true);
    View *view = create_buffer_view_generic(log_buffer, (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 800, 400}, state);
    view->bv.cursor.pos = cursor_pos_to_end_of_buffer(log_buffer, view->bv.cursor.pos);
    viewport_snap_to_cursor(log_buffer, view->bv.cursor.pos, &view->bv.viewport, &state->render_state);
    return true;
}

bool action_change_working_dir(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    View *view = create_buffer_view_prompt(
        "Change working dir:",
        prompt_create_context_change_working_dir(),
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 100},
        state);
    Text_Line current_path_line = text_line_make_f("%s", state->working_dir);
    text_buffer_insert_line(&view->bv.buffer->text_buffer, current_path_line, 1);
    view->bv.cursor.pos = cursor_pos_to_end_of_line(view->bv.buffer->text_buffer, (Cursor_Pos){1, 0});
    return true;
}

bool action_rebuild_live_scene(Editor_State *state)
{
    // TODO: view level action
    View *view = state->active_view;
    if (view->kind == VIEW_KIND_LIVE_SCENE)
    {
        live_scene_rebuild(view->lsv.live_scene);
    }
    else if (view->kind == VIEW_KIND_BUFFER &&
        view->bv.buffer->kind == BUFFER_KIND_FILE &&
        view->bv.buffer->file.linked_live_scene)
    {
        live_scene_rebuild(view->bv.buffer->file.linked_live_scene);
    }
    return true;
}

bool action_reset_live_scene(Editor_State *state)
{
    // TODO: view level action
    View *view = state->active_view;
    if (view->kind == VIEW_KIND_LIVE_SCENE)
    {
        live_scene_reset(state, &view->lsv.live_scene, view->outer_rect.w, view->outer_rect.h);
    }
    else if (view->kind == VIEW_KIND_BUFFER &&
        view->bv.buffer->kind == BUFFER_KIND_FILE &&
        view->bv.buffer->file.linked_live_scene)
    {
        live_scene_reset(state, &view->lsv.live_scene, view->outer_rect.w, view->outer_rect.h);
    }
    return true;
}

bool action_link_live_scene(Editor_State *state)
{
    View *view = state->active_view;
    if (view->kind == VIEW_KIND_BUFFER &&
        view->bv.buffer->kind == BUFFER_KIND_FILE &&
        state->view_count > 1 &&
        state->views[1]->kind == VIEW_KIND_LIVE_SCENE)
    {
        view->bv.buffer->file.linked_live_scene = state->views[1]->lsv.live_scene;
        trace_log("Linked live scene to buffer %s", view->bv.buffer->file.info.path);
    }
    return true;
}

bool action_debug_break(Editor_State *state)
{
    state->should_break = true;
    return true;
}

bool action_destroy_active_view(Editor_State *state)
{
    // TODO: Should this be view level action?
    //       then it doesn't have to target "active" specifically
    //       and do this weird "will_propagate_to_view" logic.
    //       Although, it does make sense, to "kill view" from outside the view
    if (state->active_view != NULL)
    {
        view_destroy(state->active_view, state);
    }
    return true;
}

bool action_open_test_file1(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_open_file(
        FILE_PATH1,
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
        state);
    return true;
}

bool action_open_test_image(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_image_view(
        IMAGE_PATH,
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
        state);
    return true;
}

bool action_open_test_live_scene(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_live_scene_view(
        LIVE_CUBE_PATH,
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
        state);
    return true;
}

bool action_prompt_open_file(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_prompt(
        "Open file:",
        prompt_create_context_open_file(),
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
        state);
    return true;
}

bool action_prompt_new_file(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_empty_file(
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
        state);
    return true;
}

bool action_buffer_view_move_cursor(Editor_State *state, Buffer_View *buffer_view, Cursor_Movement_Dir dir, bool with_shift, bool with_alt, bool with_super)
{
    if (with_shift && !buffer_view->mark.active) buffer_view_set_mark(buffer_view, buffer_view->cursor.pos);

    switch (dir)
    {
        case CURSOR_MOVE_LEFT:
        {
            if (with_alt) buffer_view->cursor.pos = cursor_pos_to_prev_start_of_word(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (with_super) buffer_view->cursor.pos = cursor_pos_to_indent_or_start_of_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1, true);
        } break;
        case CURSOR_MOVE_RIGHT:
        {
            if (with_alt) buffer_view->cursor.pos = cursor_pos_to_next_end_of_word(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (with_super) buffer_view->cursor.pos = cursor_pos_to_end_of_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1, true);
        } break;
        case CURSOR_MOVE_UP:
        {
            if (with_alt) buffer_view->cursor.pos = cursor_pos_to_prev_start_of_paragraph(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (with_super) buffer_view->cursor.pos = cursor_pos_to_start_of_buffer(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1);
        } break;
        case CURSOR_MOVE_DOWN:
        {
            if (with_alt) buffer_view->cursor.pos = cursor_pos_to_next_start_of_paragraph(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (with_super) buffer_view->cursor.pos = cursor_pos_to_end_of_buffer(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1);
        } break;
    }

    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    buffer_view->cursor.blink_time = 0.0f;

    if (with_shift) buffer_view_validate_mark(buffer_view);
    else buffer_view->mark.active = false;

    return true;
}

bool action_buffer_view_prompt_submit(Editor_State *state, Buffer_View *buffer_view)
{
    Prompt_Result prompt_result = prompt_parse_result(buffer_view->buffer->text_buffer);
    if (prompt_submit(buffer_view->buffer->prompt.context, prompt_result, outer_view(buffer_view)->outer_rect, state))
        view_destroy(outer_view(buffer_view), state);
    return true;
}

bool action_buffer_view_input_char(Editor_State *state, Buffer_View *buffer_view, char c)
{
    if (buffer_view->mark.active)
    {
        action_buffer_view_delete_selected(state, buffer_view);
    }
    text_buffer_insert_char(&buffer_view->buffer->text_buffer, c, buffer_view->cursor.pos);
    if (c == '\n')
    {
        int indent_level = text_buffer_match_indent(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos.line + 1);
        buffer_view->cursor.pos = cursor_pos_clamp(
            buffer_view->buffer->text_buffer,
            (Cursor_Pos){buffer_view->cursor.pos.line + 1, indent_level});
    }
    else
    {
        buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1, true);
    }
    buffer_view->cursor.blink_time = 0.0f;
    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    return true;
}

bool action_buffer_view_delete_selected(Editor_State *state, Buffer_View *buffer_view)
{
    (void)state;
    bassert(buffer_view->mark.active);
    bassert(!cursor_pos_eq(buffer_view->mark.pos, buffer_view->cursor.pos));
    Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
    Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
    text_buffer_remove_range(&buffer_view->buffer->text_buffer, start, end);
    buffer_view->mark.active = false;
    buffer_view->cursor.pos = start;
    return true;
}

bool action_buffer_view_backspace(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        action_buffer_view_delete_selected(state, buffer_view);
    }
    else
    {
        if (buffer_view->cursor.pos.line > 0 || buffer_view->cursor.pos.col > 0)
        {
            buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1, true);
            text_buffer_remove_char(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            buffer_view->cursor.blink_time = 0.0f;
            viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
        }
    }
    return true;
}

bool action_buffer_view_insert_indent(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        action_buffer_view_increase_indent_level(state, buffer_view);
    }
    else
    {
        int spaces_to_insert = INDENT_SPACES - buffer_view->cursor.pos.col % INDENT_SPACES;
        for (int i = 0; i < spaces_to_insert; i++)
        {
            text_buffer_insert_char(&buffer_view->buffer->text_buffer, ' ', buffer_view->cursor.pos);
        }
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, spaces_to_insert, +1, false);
        buffer_view->cursor.blink_time = 0.0f;
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    }
    return true;
}

bool action_buffer_view_decrease_indent_level(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        for (int i = start.line; i <= end.line; i++)
        {
            int chars_removed = text_line_indent_level_decrease(&buffer_view->buffer->text_buffer.lines[i]);
            if (i == buffer_view->mark.pos.line) buffer_view->mark.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->mark.pos, chars_removed, -1, false);
            if (i == buffer_view->cursor.pos.line) buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_removed, -1, false);
        }
    }
    else
    {
        int chars_removed = text_line_indent_level_decrease(&buffer_view->buffer->text_buffer.lines[buffer_view->cursor.pos.line]);
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_removed, -1, false);
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    }
    return true;
}

bool action_buffer_view_increase_indent_level(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        for (int i = start.line; i <= end.line; i++)
        {
            int chars_added = text_line_indent_level_increase(&buffer_view->buffer->text_buffer.lines[i]);
            if (i == buffer_view->mark.pos.line) buffer_view->mark.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->mark.pos, chars_added, +1, false);
            if (i == buffer_view->cursor.pos.line) buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_added, +1, false);
        }
    }
    else
    {
        int chars_added = text_line_indent_level_increase(&buffer_view->buffer->text_buffer.lines[buffer_view->cursor.pos.line]);
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_added, +1, false);
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    }
    return true;
}

bool action_buffer_view_copy_selected(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        if (ENABLE_OS_CLIPBOARD)
        {
            char *range = text_buffer_extract_range(&buffer_view->buffer->text_buffer, start, end);
            write_clipboard_mac(range);
            free(range);
        }
        else
        {
            if (state->copy_buffer) free(state->copy_buffer);
            state->copy_buffer = text_buffer_extract_range(&buffer_view->buffer->text_buffer, start, end);
        }
    }
    return true;
}

bool action_buffer_view_paste(Editor_State *state, Buffer_View *buffer_view)
{
    if (ENABLE_OS_CLIPBOARD)
    {
        char buf[10*1024];
        read_clipboard_mac(buf, sizeof(buf));
        if (strlen(buf) > 0)
        {
            if (buffer_view->mark.active)
            {
                action_buffer_view_delete_selected(state, buffer_view);
            }
            Cursor_Pos new_cursor = text_buffer_insert_range(&buffer_view->buffer->text_buffer, buf, buffer_view->cursor.pos);
            buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, new_cursor);
        }
    }
    else
    {
        if (state->copy_buffer)
        {
            if (buffer_view->mark.active)
            {
                action_buffer_view_delete_selected(state, buffer_view);
            }
            Cursor_Pos new_cursor = text_buffer_insert_range(&buffer_view->buffer->text_buffer, state->copy_buffer, buffer_view->cursor.pos);
            buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, new_cursor);
        }
    }
    return true;
}

bool action_buffer_view_delete_current_line(Editor_State *state, Buffer_View *buffer_view)
{
    text_buffer_remove_line(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos.line);
    buffer_view->cursor.pos = cursor_pos_to_start_of_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    return true;
}

bool action_buffer_view_reload_file(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->buffer->file.info.path != NULL)
    {
        Buffer *new_buffer = buffer_create_read_file(buffer_view->buffer->file.info.path, state);
        buffer_destroy(buffer_view->buffer, state);
        buffer_view->buffer = new_buffer;
    }
    return true;
}

bool action_buffer_view_prompt_save_file_as(Editor_State *state, Buffer_View *buffer_view)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_prompt(
        "Save as:",
        prompt_create_context_save_as(buffer_view),
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
        state);
    return true;
}

bool action_buffer_view_save_file(Editor_State *state, Buffer_View *buffer_view)
{
    (void)state;
    bassert(buffer_view->buffer->file.info.path);
    file_write(buffer_view->buffer->text_buffer, buffer_view->buffer->file.info.path);
    buffer_view->buffer->file.info.has_been_modified = false;
    return true;
}

bool action_buffer_view_change_zoom(Editor_State *state, Buffer_View *buffer_view, float amount)
{
    (void)state;
    buffer_view->viewport.zoom += amount;
    return true;
}

bool action_buffer_view_prompt_go_to_line(Editor_State *state, Buffer_View *buffer_view)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_prompt(
        "Go to line:",
        prompt_create_context_go_to_line(buffer_view),
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
        state);
    return true;
}

bool action_buffer_view_prompt_search_next(Editor_State *state, Buffer_View *buffer_view)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    View *prompt_view = create_buffer_view_prompt(
        "Search next:",
        prompt_create_context_search_next(buffer_view),
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
        state);
    if (state->prev_search)
    {
        Text_Line current_path_line = text_line_make_f("%s", state->prev_search);
        text_buffer_insert_line(&prompt_view->bv.buffer->text_buffer, current_path_line, 1);
        prompt_view->bv.cursor.pos = cursor_pos_to_end_of_line(prompt_view->bv.buffer->text_buffer, (Cursor_Pos){1, 0});
    }
    return true;
}

bool action_buffer_view_repeat_search(Editor_State *state, Buffer_View *buffer_view)
{
    if (state->prev_search)
    {
        Cursor_Pos found_pos;
        bool found = text_buffer_search_next(&buffer_view->buffer->text_buffer, state->prev_search, buffer_view->cursor.pos, &found_pos);
        if (found)
        {
            buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, found_pos);
            viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
            buffer_view->cursor.blink_time = 0.0f;
        }
    }
    return true;
}

bool action_buffer_view_whitespace_cleanup(Editor_State *state, Buffer_View *buffer_view)
{
    (void)state;
    int cleaned_lines = text_buffer_whitespace_cleanup(&buffer_view->buffer->text_buffer);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    trace_log("buffer_view_whitespace_cleanup: Cleaned up %d lines", cleaned_lines);
    return true;
}
