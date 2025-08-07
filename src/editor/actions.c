#include "actions.h"

#include "../lib/common.h"

#include <GLFW/glfw3.h>

#include "editor.h"
#include "history.h"
#include "../lib/string_builder.h"
#include "text_buffer.h"
#include "unit_tests.h"
#include "../lib/util.h"

bool action_run_unit_tests(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    Text_Buffer log_buffer = {0};
    unit_tests_run(&log_buffer, true);
    View *view = create_buffer_view_generic((Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 800, 400}, state);
    buffer_replace_text_buffer(view->bv.buffer, log_buffer);
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

bool action_prompt_open_file(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_prompt(
        "Open file:",
        prompt_create_context_open_file(),
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 400, 100},
        state);
    return true;
}

bool action_prompt_new_file(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    create_buffer_view_generic(
        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
        state);
    return true;
}

const char *_action_save_workspace_get_view_kind_str(View_Kind kind)
{
    switch (kind)
    {
        case VIEW_KIND_BUFFER:
        {
            return "BUFFER";
        } break;

        case VIEW_KIND_IMAGE:
        {
            return "IMAGE";
        } break;
    }
}

void _action_save_workspace_save_text_buffer_to_temp_loc(Text_Buffer tb, String_Builder *sb)
{
    time_t now = time(NULL);
    int r = rand() % 100000;
    char *temp_path = strf(E2_TEMP_FILES "/%ld_%d.tmp", now, r);
    text_buffer_write_to_file(tb, temp_path);
    string_builder_append_f(sb, "  TEMP_PATH='%s'\n", temp_path);
    free(temp_path);
}

bool action_save_workspace(Editor_State *state)
{
    clear_dir(E2_TEMP_FILES);

    String_Builder sb = {0};

    string_builder_append_f(&sb, "WORK_DIR='%s'\n", state->working_dir);
    string_builder_append_f(&sb, "CANVAS_POS=(%.3f,%.3f)\n", state->canvas_viewport.rect.x, state->canvas_viewport.rect.y);
    string_builder_append_f(&sb, "BUF_SEED=%d\n", state->buffer_seed);

    // Save views in reverse order, so it's easier to load (last loaded view is in the front of the queue)
    for (int i = state->view_count - 1; i >= 0; i--)
    {
        View *view = state->views[i];
        string_builder_append_f(&sb, "VIEW={\n");
        {
            string_builder_append_f(&sb, "  KIND=%s\n", _action_save_workspace_get_view_kind_str(view->kind));
            string_builder_append_f(&sb, "  RECT=(%.3f,%.3f,%.3f,%.3f)\n", view->outer_rect.x, view->outer_rect.y, view->outer_rect.w, view->outer_rect.h);

            switch (view->kind)
            {
                case VIEW_KIND_BUFFER:
                {
                    Buffer_View *bv = &view->bv;
                    string_builder_append_f(&sb, "  BUF_ID=%d\n", bv->buffer->id);
                    string_builder_append_f(&sb, "  BUF_VIEWPORT=(%.3f,%.3f,%.3f)\n", bv->viewport.rect.x, bv->viewport.rect.y, bv->viewport.zoom);
                    string_builder_append_f(&sb, "  CURSOR=(%d,%d)\n", bv->cursor.pos.line, bv->cursor.pos.col);
                    if (bv->mark.active)
                    {
                        string_builder_append_f(&sb, "  MARK=(%d,%d)\n", bv->mark.pos.line, bv->mark.pos.col);
                    }

                    Buffer *b = bv->buffer;
                    if (b->prompt_context.kind == PROMPT_NONE) // Don't save prompt views
                    {
                        _action_save_workspace_save_text_buffer_to_temp_loc(b->text_buffer, &sb);
                        if (b->file_path)
                        {
                            string_builder_append_f(&sb, "  FILE_PATH='%s'\n", b->file_path);
                        }
                    }
                } break;

                case VIEW_KIND_IMAGE:
                {
                    log_warning("Image view saving not implemented");
                } break;
            }
        }
        string_builder_append_f(&sb, "}\n");
    }

    char *content = string_builder_compile_and_destroy(&sb);
    file_write(E2_WORKSPACE, content);
    free(content);

    return true;
}

#include "ws_parser.h"

#define LOG_AND_RET(log) do { log_warning(log); return false; } while(0)

bool _action_load_workspace_parse_view_kvs(Parser_State *ps, Editor_State *state)
{
    View_Kind view_kind = 0;
    bool has_view_kind = false;
    Rect rect = {0};
    bool has_rect = false;
    int buf_id = 0;
    bool has_buf_id = false;
    float buf_viewport_x = 0;
    float buf_viewport_y = 0;
    float buf_viewport_zoom = 0;
    bool has_buf_viewport = false;
    Cursor_Pos cursor = {0};
    bool has_cursor = false;
    Cursor_Pos mark = {0};
    bool has_mark = false;
    char *file_path = NULL;
    bool has_file_path = false;
    char *temp_path = NULL;
    bool has_temp_path = false;
    int action_scratch_buffer_id = 0;
    bool has_action_scratch_buffer_id = false;

    while (1)
    {
        Token key = ws_parser_next_token(ps);
        if (key.kind == TOKEN_RBRACE) break;
        if (key.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");

        if (ws_parser_next_token(ps).kind != TOKEN_EQUALS) LOG_AND_RET("Expected EQUALS.");

        if (key.length == 4 && strncmp(key.start, "KIND", 4) == 0)
        {
            Token val = ws_parser_next_token(ps);
            if (val.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");

            if (val.length == 6 && strncmp(val.start, "BUFFER", 6) == 0) view_kind = VIEW_KIND_BUFFER;
            else if (val.length == 5 && strncmp(val.start, "IMAGE", 5) == 0) view_kind = VIEW_KIND_IMAGE;
            else LOG_AND_RET("Invalid KIND");

            has_view_kind = true;
        }
        else if (key.length == 4 && strncmp(key.start, "RECT", 4) == 0)
        {
            if (ws_parser_next_token(ps).kind != TOKEN_LPAREN) LOG_AND_RET("Expected LPAREN.");

            Token t_v;

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            rect.x = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            rect.y = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            rect.w = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            rect.h = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_RPAREN) LOG_AND_RET("Expected RPAREN.");

            has_rect = true;
        }
        else if (key.length == 6 && strncmp(key.start, "BUF_ID", 6) == 0)
        {
            Token val = ws_parser_next_token(ps);
            if (val.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            buf_id = atoi(val.start);
            has_buf_id = true;
        }
        else if (key.length == 12 && strncmp(key.start, "BUF_VIEWPORT", 12) == 0)
        {
            if (ws_parser_next_token(ps).kind != TOKEN_LPAREN) LOG_AND_RET("Expected LPAREN.");

            Token t_v;

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            buf_viewport_x = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            buf_viewport_y = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            buf_viewport_zoom = atof(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_RPAREN) LOG_AND_RET("Expected RPAREN.");

            has_buf_viewport = true;
        }
        else if (key.length == 6 && strncmp(key.start, "CURSOR", 6) == 0)
        {
            if (ws_parser_next_token(ps).kind != TOKEN_LPAREN) LOG_AND_RET("Expected LPAREN.");

            Token t_v;

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            cursor.line = atoi(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            cursor.col = atoi(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_RPAREN) LOG_AND_RET("Expected RPAREN.");

            has_cursor = true;
        }
        else if (key.length == 4 && strncmp(key.start, "MARK", 4) == 0)
        {
            if (ws_parser_next_token(ps).kind != TOKEN_LPAREN) LOG_AND_RET("Expected LPAREN.");

            Token t_v;

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            mark.line = atoi(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_COMMA) LOG_AND_RET("Expected COMMA.");

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            mark.col = atoi(t_v.start);

            if (ws_parser_next_token(ps).kind != TOKEN_RPAREN) LOG_AND_RET("Expected RPAREN.");

            has_mark = true;
        }
        else if (key.length == 9 && strncmp(key.start, "FILE_PATH", 9) == 0)
        {
            Token val = ws_parser_next_token(ps);
            if (val.kind != TOKEN_STRING) LOG_AND_RET("Expected STRING.");

            file_path = strndup(val.start, val.length);
            has_file_path = true;
        }
        else if (key.length == 9 && strncmp(key.start, "TEMP_PATH", 9) == 0)
        {
            Token val = ws_parser_next_token(ps);
            if (val.kind != TOKEN_STRING) LOG_AND_RET("Expected STRING.");

            temp_path = strndup(val.start, val.length);
            has_temp_path = true;
        }
    }

    // if (has_view_kind) trace_log("View kind: %d", view_kind);
    // if (has_rect) trace_log("Rect: %.3f, %.3f, %.3f, %.3f", rect.x, rect.y, rect.w, rect.h);
    // if (has_buf_id) trace_log("Buffer ID: %d", buf_id);
    // if (has_buf_viewport) trace_log("Buf viewport: %.3f, %.3f, %.3f", buf_viewport_x, buf_viewport_y, buf_viewport_zoom);
    // if (has_cursor) trace_log("Cursor: %d, %d", cursor.line, cursor.col);
    // if (has_mark) trace_log("Mark: %d, %d", mark.line, mark.col);
    // if (has_file_path) trace_log("File path: %s", file_path);
    // if (has_temp_path) trace_log("Temp path: %s", temp_path);
    // if (has_dl_path) trace_log("DL path: %s", dl_path);

    if (!has_view_kind) LOG_AND_RET("No view kind.");
    if (!has_rect) rect = (Rect){0, 0, 100, 100};

    switch (view_kind)
    {
        case VIEW_KIND_BUFFER:
        {
            if (!has_buf_id) LOG_AND_RET("No buffer id.");
            View *view = create_buffer_view_generic(rect, state);
            if (has_buf_viewport)
            {
                view->bv.viewport.rect.x = buf_viewport_x;
                view->bv.viewport.rect.y = buf_viewport_y;
                viewport_set_zoom(&view->bv.viewport, buf_viewport_zoom);
            }
            view->bv.buffer->id = buf_id;

            if (has_temp_path)
            {
                Text_Buffer content;
                bool read_success = text_buffer_read_from_file(temp_path, &content);
                if (read_success)
                {
                    buffer_replace_text_buffer(view->bv.buffer, content);
                }
                else
                {
                    log_warning("Failed to read from temp file at %s", temp_path);
                    return false;
                }

                free(temp_path);
            }

            if (has_file_path)
            {
                if (!has_temp_path)
                {
                    Text_Buffer content;
                    bool read_success = text_buffer_read_from_file(file_path, &content);
                    if (read_success)
                    {
                        buffer_replace_text_buffer(view->bv.buffer, content);
                    }
                    else
                    {
                        log_warning("Failed to read from file at %s", file_path);
                        return false;
                    }
                }

                view->bv.buffer->file_path = xstrdup(file_path);
                free(file_path);
            }

            if (has_cursor)
            {
                view->bv.cursor.pos = cursor_pos_clamp(view->bv.buffer->text_buffer, cursor);
            }

            if (has_mark)
            {
                view->bv.mark.pos = cursor_pos_clamp(view->bv.buffer->text_buffer, mark);
                view->bv.mark.active = true;
            }
        } break;

        case VIEW_KIND_IMAGE:
        {
            LOG_AND_RET("Image view loading not implemented");
        } break;
    }

    return true;
}

bool _action_load_workspace_parse_kvs(Parser_State *ps, Editor_State *state)
{
    while (1)
    {
        Token key = ws_parser_next_token(ps);
        if (key.kind == TOKEN_EOF) break;
        if (key.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");

        if (ws_parser_next_token(ps).kind != TOKEN_EQUALS) LOG_AND_RET("Expected EQUALS.");

        if (key.length == 8 && strncmp(key.start, "WORK_DIR", 8) == 0)
        {
            Token val = ws_parser_next_token(ps);
            if (val.kind != TOKEN_STRING) LOG_AND_RET("Expected STRING.");

            char *working_dir = strndup(val.start, val.length);
            // trace_log("Will change working dir to %s", state->working_dir);
            sys_change_working_dir(working_dir, state);
            free(working_dir);
        }
        else if (key.length == 10 && strncmp(key.start, "CANVAS_POS", 10) == 0)
        {
            if (ws_parser_next_token(ps).kind != TOKEN_LPAREN) LOG_AND_RET("Expected LPAREN.");

            Token t_v;

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            float x = atof(t_v.start);

            ws_parser_next_token(ps); // comma

            t_v = ws_parser_next_token(ps);
            if (t_v.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            float y = atof(t_v.start);

            // trace_log("Will set canvas pos to %.3f, %.3f", x, y);
            state->canvas_viewport.rect.x = x;
            state->canvas_viewport.rect.y = y;

            if (ws_parser_next_token(ps).kind != TOKEN_RPAREN) LOG_AND_RET("Expected RPAREN.");
        }
        else if (key.length == 8 && strncmp(key.start, "BUF_SEED", 8) == 0)
        {
            Token value = ws_parser_next_token(ps);;
            if (value.kind != TOKEN_IDENT) LOG_AND_RET("Expected IDENT.");
            int buffer_seed = atoi(value.start);

            // trace_log("Will set buffer seed to %d", buffer_seed);
            state->buffer_seed = buffer_seed;
        }
        else if (key.length == 4 && strncmp(key.start, "VIEW", 4) == 0)
        {
            if (ws_parser_next_token(ps).kind != TOKEN_LBRACE) LOG_AND_RET("Expected LBRACE.");

            // TODO: HACK: Save buffer seed temporarily, as creating buffer views will adjust the seed, but then the id gets replaced with the one from the save file.
            int buffer_seed = state->buffer_seed;

            // trace_log("Will create view:");
            if (!_action_load_workspace_parse_view_kvs(ps, state)) LOG_AND_RET("Failed to parse View.");

            state->buffer_seed = buffer_seed;
        }
    }

    return true;
}

bool action_load_workspace(Editor_State *state)
{
    if (!sys_file_exists(E2_WORKSPACE)) return false;

    char *workspace = read_file(E2_WORKSPACE, NULL);
    if (!workspace) return false;

    Parser_State parser_state = { .src = workspace };
    return _action_load_workspace_parse_kvs(&parser_state, state);
}

bool action_reload_workspace(Editor_State *state)
{
    for (int i = 0; i < state->view_count; i++)
    {
        view_destroy(state->views[0], state);
    }

    return action_load_workspace(state);
}

bool action_temp_load_debug_scene(Editor_State *state)
{
    state->hub_context->open_scene("bin/debug.dylib");
    return true;
}

bool action_temp_load_cube_scene(Editor_State *state)
{
    state->hub_context->open_scene("bin/cube.dylib");
    return true;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

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
    if (prompt_submit(buffer_view->buffer->prompt_context, prompt_result, outer_view(buffer_view)->outer_rect, state))
        view_destroy(outer_view(buffer_view), state);
    return true;
}

bool action_buffer_view_input_char(Editor_State *state, Buffer_View *buffer_view, char c)
{
    Command *last_uncommitted_command = history_get_last_uncommitted_command(&buffer_view->buffer->history);
    if (last_uncommitted_command && last_uncommitted_command->delta_count > 0)
    {
        Delta *prev_delta = &last_uncommitted_command->deltas[last_uncommitted_command->delta_count - 1];
        if (prev_delta->kind == DELTA_INSERT_CHAR &&
            (!isalnum(prev_delta->insert_char.c) && isalnum(c)))
        {
            // If a word edge has been encountered, commit command
            history_commit_command(&buffer_view->buffer->history);
        }
    }

    history_begin_command_running(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Text insert", RUNNING_COMMAND_TEXT_INSERT);

    if (buffer_view->mark.active)
    {
        action_buffer_view_delete_selected(state, buffer_view);
    }

    text_buffer_history_insert_char(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, c, buffer_view->cursor.pos);
    if (c == '\n')
    {
        int indent_level = text_buffer_history_line_match_indent(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, buffer_view->cursor.pos.line + 1);
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

    bool new_command = history_begin_command_non_interrupt(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Delete selected");

    Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
    Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
    text_buffer_history_remove_range(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, start, end);

    if (new_command) history_commit_command(&buffer_view->buffer->history);

    buffer_view->mark.active = false;
    buffer_view->cursor.pos = start;
    return true;
}

bool action_buffer_view_backspace(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        history_begin_command_running(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Text deletion", RUNNING_COMMAND_TEXT_DELETION);
        return action_buffer_view_delete_selected(state, buffer_view);
    }
    else
    {
        if (buffer_view->cursor.pos.line > 0 || buffer_view->cursor.pos.col > 0)
        {
            Cursor_Pos prev_cursor_pos = buffer_view->cursor.pos;
            buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1, true);
            char c = text_buffer_get_char(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            if (c == '\n')
            {
                // Deleted line break, commit current command, before starting new one
                history_commit_command(&buffer_view->buffer->history);
            }

            history_begin_command_running(&buffer_view->buffer->history, prev_cursor_pos, buffer_view->mark, "Text deletion", RUNNING_COMMAND_TEXT_DELETION);

            text_buffer_history_remove_char(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, buffer_view->cursor.pos);

            buffer_view->cursor.blink_time = 0.0f;
            viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
        }
    }
    return true;
}

bool action_buffer_view_backspace_word(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        return action_buffer_view_delete_selected(state, buffer_view);
    }
    else
    {
        if (buffer_view->cursor.pos.line > 0 || buffer_view->cursor.pos.col > 0)
        {
            bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Backspace word");

            Text_Buffer *tb = &buffer_view->buffer->text_buffer;
            Cursor_Pos cursor = buffer_view->cursor.pos;
            Cursor_Pos word_start = cursor_pos_to_prev_start_of_word(*tb, cursor);
            text_buffer_history_remove_range(tb, &buffer_view->buffer->history, word_start, cursor);

            buffer_view->cursor.pos = word_start;
            buffer_view->cursor.blink_time = 0.0f;
            viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);

            if (new_command) history_commit_command(&buffer_view->buffer->history);
        }
    }

    return true;
}

bool action_buffer_view_insert_indent(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        return action_buffer_view_increase_indent_level(state, buffer_view);
    }
    else
    {
        bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Insert indent");
        int spaces_to_insert = INDENT_SPACES - buffer_view->cursor.pos.col % INDENT_SPACES;
        for (int i = 0; i < spaces_to_insert; i++)
        {
            text_buffer_history_insert_char(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, ' ', buffer_view->cursor.pos);
        }
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, spaces_to_insert, +1, false);
        buffer_view->cursor.blink_time = 0.0f;
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);

        if (new_command) history_commit_command(&buffer_view->buffer->history);
    }
    return true;
}

bool action_buffer_view_decrease_indent_level(Editor_State *state, Buffer_View *buffer_view)
{
    bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Decrease indent");

    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        // If the cursor is at the start of the next linen, a multi-line operation shouldn't be done on that line
        if (end.col == 0 && end.line > 0)
        {
            end.line--;
        }
        for (int i = start.line; i <= end.line; i++)
        {
            int chars_removed = text_buffer_history_line_indent_decrease_level(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, i);
            if (i == buffer_view->mark.pos.line) buffer_view->mark.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->mark.pos, chars_removed, -1, false);
            if (i == buffer_view->cursor.pos.line) buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_removed, -1, false);
        }
    }
    else
    {
        int chars_removed = text_buffer_history_line_indent_decrease_level(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, buffer_view->cursor.pos.line);
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_removed, -1, false);
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    }

    if (new_command) history_commit_command(&buffer_view->buffer->history);

    return true;
}

bool action_buffer_view_increase_indent_level(Editor_State *state, Buffer_View *buffer_view)
{
    bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Increase indent");

    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        // If the cursor is at the start of the next linen, a multi-line operation shouldn't be done on that line
        if (end.col == 0 && end.line > 0)
        {
            end.line--;
        }
        for (int i = start.line; i <= end.line; i++)
        {
            int chars_added = text_buffer_history_line_indent_increase_level(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, i);
            if (i == buffer_view->mark.pos.line) buffer_view->mark.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->mark.pos, chars_added, +1, false);
            if (i == buffer_view->cursor.pos.line) buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_added, +1, false);
        }
    }
    else
    {
        int chars_added = text_buffer_history_line_indent_increase_level(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, buffer_view->cursor.pos.line);
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_added, +1, false);
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    }

    if (new_command) history_commit_command(&buffer_view->buffer->history);

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
        return true;
    }

    return false;
}

bool action_buffer_view_cut_selected(Editor_State *state, Buffer_View *buffer_view)
{
    if (buffer_view->mark.active)
    {
        bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Cut selected");

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

        action_buffer_view_delete_selected(state, buffer_view);

        if (new_command) history_commit_command(&buffer_view->buffer->history);
    }

    return false;
}

bool action_buffer_view_paste(Editor_State *state, Buffer_View *buffer_view)
{
    char *copy_buffer = NULL;
    if (ENABLE_OS_CLIPBOARD)
    {
        char buf[10*1024];
        read_clipboard_mac(buf, sizeof(buf));
        if (strlen(buf) > 0)
        {
            copy_buffer = buf;
        }
    }
    else
    {
        copy_buffer = state->copy_buffer;
    }

    if (copy_buffer)
    {
        bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Paste");

        if (buffer_view->mark.active)
        {
            action_buffer_view_delete_selected(state, buffer_view);
        }
        Cursor_Pos start = buffer_view->cursor.pos;
        Cursor_Pos end = text_buffer_history_insert_range(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, copy_buffer, buffer_view->cursor.pos);
        buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, end);

        if (new_command) history_commit_command(&buffer_view->buffer->history);
        return true;
    }

    return false;
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
    if (buffer_view->buffer->file_path)
    {
        Text_Buffer tb;
        text_buffer_read_from_file(buffer_view->buffer->file_path, &tb);
        buffer_replace_text_buffer(buffer_view->buffer, tb);
        buffer_view->cursor.pos = cursor_pos_clamp(tb, buffer_view->cursor.pos);
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
    if (buffer_view->buffer->file_path)
    {
        text_buffer_history_whitespace_cleanup(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history);
        buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
        text_buffer_write_to_file(buffer_view->buffer->text_buffer, buffer_view->buffer->file_path);
        action_save_workspace(state);
    }
    else
    {
        action_buffer_view_prompt_save_file_as(state, buffer_view);
    }
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
    bool new_command = history_begin_command(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Whitespace cleanup");

    int cleaned_lines = text_buffer_history_whitespace_cleanup(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    trace_log("buffer_view_whitespace_cleanup: Cleaned up %d lines", cleaned_lines);

    if (new_command) history_commit_command(&buffer_view->buffer->history);

    return true;
}

bool action_buffer_view_view_history(Editor_State *state, Buffer_View *buffer_view)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);;
    Text_Buffer history_tb = {0};
    text_buffer_append_f(&history_tb, "History. Pos: %d. Count: %d", buffer_view->buffer->history.history_pos, buffer_view->buffer->history.command_count);
    for (int command_i = 0; command_i < buffer_view->buffer->history.command_count; command_i++)
    {
        Command *c = &buffer_view->buffer->history.commands[command_i];
        text_buffer_append_f(&history_tb, "%03d: '%s' (committed: %s; initial pos: %d, %d)",
            command_i,
            c->name,
            c->committed ? "true" : "false",
            c->cursor_pos.line, c->cursor_pos.col);
        for (int delta_i = 0; delta_i < c->delta_count; delta_i++)
        {
            Delta *d = &c->deltas[delta_i];
            switch (d->kind)
            {
                case DELTA_INSERT_CHAR:
                {
                    text_buffer_append_f(&history_tb, "  %s: %c (%d, %d)",
                        DeltaKind_Str[d->kind],
                        d->insert_char.c,
                        d->insert_char.pos.line, d->insert_char.pos.col);
                } break;

                case DELTA_REMOVE_CHAR:
                {
                    text_buffer_append_f(&history_tb, "  %s: %c (%d, %d)",
                        DeltaKind_Str[d->kind],
                        d->remove_char.c,
                        d->remove_char.pos.line, d->remove_char.pos.col);
                } break;

                case DELTA_INSERT_RANGE:
                {
                    text_buffer_append_f(&history_tb, "  %s: '%s' (%d, %d -> %d, %d)",
                        DeltaKind_Str[d->kind],
                        d->insert_range.range,
                        d->insert_range.start.line, d->insert_range.start.col,
                        d->insert_range.end.line, d->insert_range.end.col);
                } break;

                case DELTA_REMOVE_RANGE:
                {
                    text_buffer_append_f(&history_tb, "  %s: '%s' (%d, %d -> %d, %d)",
                        DeltaKind_Str[d->kind],
                        d->remove_range.range,
                        d->remove_range.start.line, d->remove_range.start.col,
                        d->remove_range.end.line, d->remove_range.end.col);
                } break;
            }
        }
    }
    View *view = create_buffer_view_generic((Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 800, 400}, state);
    buffer_replace_text_buffer(view->bv.buffer, history_tb);
    return true;
}

bool action_buffer_view_undo_command(Editor_State *state, Buffer_View *buffer_view)
{
    if (history_get_last_uncommitted_command(&buffer_view->buffer->history))
    {
        history_commit_command(&buffer_view->buffer->history);
    }

    Command *command = history_get_command_to_undo(&buffer_view->buffer->history);
    if (command)
    {
        bassert(command->delta_count > 0);

        history_begin_command_non_reset(&buffer_view->buffer->history, buffer_view->cursor.pos, buffer_view->mark, "Undo action");
        command = history_get_command_to_undo(&buffer_view->buffer->history);  // Copy because begin_command could realloc

        for (int i = command->delta_count  - 1; i>= 0; i--)
        {
            Delta *delta = &command->deltas[i];
            switch (delta->kind)
            {
                case DELTA_INSERT_CHAR:
                {
                    text_buffer_history_remove_char(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, delta->insert_char.pos);
                } break;

                case DELTA_REMOVE_CHAR:
                {
                    text_buffer_history_insert_char(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, delta->remove_char.c, delta->remove_char.pos);
                } break;

                case DELTA_INSERT_RANGE:
                {
                    text_buffer_history_remove_range(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, delta->insert_range.start, delta->insert_range.end);
                } break;

                case DELTA_REMOVE_RANGE:
                {
                    text_buffer_history_insert_range(&buffer_view->buffer->text_buffer, &buffer_view->buffer->history, delta->remove_range.range, delta->remove_range.start);
                } break;
            }
        }

        history_commit_command(&buffer_view->buffer->history);
        buffer_view->buffer->history.history_pos--;

        buffer_view->mark = command->mark;
        buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, command->cursor_pos);
        buffer_view->cursor.blink_time = 0.0f;
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);

        return true;
    }
    return false;
}

// ----------------------------------------------
