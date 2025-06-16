#include "input.h"

#include <stdbool.h>

#include "actions.h"
#include "editor.h"
#include "util.h"

void buffer_view_handle_key(Buffer_View *buffer_view, Frame *frame, GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    (void)window;
    switch(key)
    {
        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
        if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            Cursor_Movement_Dir dir = CURSOR_MOVE_UP;
            switch (key)
            {
                case GLFW_KEY_LEFT: dir = CURSOR_MOVE_LEFT; break;
                case GLFW_KEY_RIGHT: dir = CURSOR_MOVE_RIGHT; break;
                case GLFW_KEY_UP: dir = CURSOR_MOVE_UP; break;
                case GLFW_KEY_DOWN: dir = CURSOR_MOVE_DOWN; break;
            }
            action_buffer_view_move_cursor(state, buffer_view, dir, mods & GLFW_MOD_SHIFT, mods & GLFW_MOD_ALT, mods & GLFW_MOD_SUPER);
        } break;
        case GLFW_KEY_ENTER: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            switch (buffer_view->buffer->kind)
            {
                case BUFFER_KIND_PROMPT:
                {
                    action_buffer_view_prompt_submit(state, buffer_view, frame);
                } break;

                default:
                {
                    action_buffer_view_input_char(state, buffer_view, '\n');
                } break;
            }
        } break;
        case GLFW_KEY_BACKSPACE: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            action_buffer_view_backspace(state, buffer_view);
        } break;
        case GLFW_KEY_TAB: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            action_buffer_view_insert_indent(state, buffer_view);
        } break;
        case GLFW_KEY_LEFT_BRACKET: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            action_buffer_view_decrease_indent_level(state, buffer_view);
        } break;
        case GLFW_KEY_RIGHT_BRACKET: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            action_buffer_view_increase_indent_level(state, buffer_view);
        } break;
        case GLFW_KEY_C: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_buffer_view_copy_selected(state, buffer_view);
        } break;
        case GLFW_KEY_V: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_buffer_view_paste(state, buffer_view);
        } break;
        case GLFW_KEY_X: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            action_buffer_view_delete_current_line(state, buffer_view);
        } break;
        case GLFW_KEY_R: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (buffer_view->buffer->kind == BUFFER_KIND_FILE)
            {
                action_buffer_view_reload_file(state, buffer_view);
            }
        } break;
        case GLFW_KEY_S: if (mods & GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (buffer_view->buffer->kind == BUFFER_KIND_FILE)
            {
                if (mods & GLFW_MOD_SHIFT || buffer_view->buffer->file.info.path == NULL)
                {
                    action_buffer_view_prompt_save_file_as(state, buffer_view);
                }
                else
                {
                    action_buffer_view_save_file(state, buffer_view);
                }
            }
        } break;
        case GLFW_KEY_EQUAL: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            action_buffer_view_change_zoom(state, buffer_view, 0.25f);
        } break;
        case GLFW_KEY_MINUS: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            action_buffer_view_change_zoom(state, buffer_view, -0.25f);
        } break;
        case GLFW_KEY_G: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_buffer_view_prompt_go_to_line(state, buffer_view);
        } break;
        case GLFW_KEY_F: if (mods & GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            if (mods == (GLFW_MOD_SHIFT | GLFW_MOD_SUPER))
            {
                action_buffer_view_repeat_search(state, buffer_view);
            }
            else if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
            {
                action_buffer_view_prompt_search_next(state, buffer_view);
            }
        } break;
        case GLFW_KEY_SEMICOLON: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_buffer_view_whitespace_cleanup(state, buffer_view);
        } break;
    }
}

void view_handle_key(View *view, Frame *frame, GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            buffer_view_handle_key(&view->bv, frame, window, state, key, action, mods);
        } break;

        case VIEW_KIND_IMAGE:
        {
            // Nothing for now
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            // Nothing for now
        } break;

        default:
        {
            log_warning("view_handle_key: Unhandled View kind: %d", view->kind);
        } break;
    }
}

void handle_key_input(GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    (void) window;
    bool will_propagate_to_view = true;
    switch(key)
    {
        case GLFW_KEY_F1: if (action == GLFW_PRESS)
        {
            action_run_unit_tests(state);
        } break;
        case GLFW_KEY_F2: if (action == GLFW_PRESS)
        {
            action_change_working_dir(state);
        } break;
        case GLFW_KEY_F5: if (action == GLFW_PRESS)
        {
            if (mods == 0)
                action_rebuild_live_scene(state);
            else if (mods == GLFW_MOD_SHIFT)
                action_reset_live_scene(state);
        } break;
        case GLFW_KEY_F6: if (action == GLFW_PRESS)
        {
            action_link_live_scene(state);
        } break;
        case GLFW_KEY_F12: if (action == GLFW_PRESS)
        {
            action_debug_break(state);
        } break;
        case GLFW_KEY_W: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            will_propagate_to_view = false;
            action_destroy_active_frame(state);
        } break;
        case GLFW_KEY_1: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_open_test_file1(state);
        } break;
        case GLFW_KEY_2: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_open_test_image(state);
        } break;
        case GLFW_KEY_3: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_open_test_live_scene(state);
        } break;
        case GLFW_KEY_O: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_prompt_open_file(state);
        } break;
        case GLFW_KEY_N: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            action_prompt_new_file(state);
        } break;
    }

    if (will_propagate_to_view && state->active_frame)
    {
        view_handle_key(state->active_frame->view, state->active_frame, window, state, key, action, mods);
    }
}

void buffer_view_handle_click_drag(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    (void)is_shift_pressed;
    buffer_view___set_cursor_to_pixel_position(buffer_view, frame_rect, mouse_canvas_pos, render_state);
}

void view_handle_click_drag(View *view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        buffer_view_handle_click_drag(&view->bv, frame_rect, mouse_canvas_pos, is_shift_pressed, render_state);
    }
}

void frame_handle_drag(Frame *frame, Vec_2 drag_delta, Render_State *render_state)
{
    Rect new_rect = frame->outer_rect;
    new_rect.x += drag_delta.x;
    new_rect.y += drag_delta.y;
    frame_set_rect(frame, new_rect, render_state);
}

void frame_handle_resize(Frame *frame, Vec_2 drag_delta, Render_State *render_state)
{
    Rect new_rect = frame->outer_rect;
    new_rect.w += drag_delta.x;
    new_rect.h += drag_delta.y;
    frame_set_rect(frame, new_rect, render_state);
}

void handle_mouse_click_drag(Vec_2 mouse_canvas_pos, Vec_2 mouse_delta, bool is_shift_pressed, Mouse_State *mouse_state, Render_State *render_state)
{
    if (mouse_state->drag_in_view_frame)
    {
        view_handle_click_drag(mouse_state->drag_in_view_frame->view, mouse_state->drag_in_view_frame->outer_rect, mouse_canvas_pos, is_shift_pressed, render_state);
    }
    else if (mouse_state->dragged_frame)
    {
        frame_handle_drag(mouse_state->dragged_frame, mouse_delta, render_state);
    }
    else if (mouse_state->resized_frame)
    {
        frame_handle_resize(mouse_state->resized_frame, mouse_delta, render_state);
    }
}

bool buffer_view_handle_mouse_click(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(*buffer_view, frame_rect, render_state);
    if (rect_p_intersect(mouse_canvas_pos, text_area_rect))
    {
        if (is_shift_pressed)
        {
            if (!buffer_view->mark.active)
                buffer_view___set_mark(buffer_view, buffer_view->cursor.pos);

            buffer_view___set_cursor_to_pixel_position(buffer_view, frame_rect, mouse_canvas_pos, render_state);

            buffer_view___validate_mark(buffer_view);
        }
        else
        {
            buffer_view___set_cursor_to_pixel_position(buffer_view, frame_rect, mouse_canvas_pos, render_state);
            buffer_view___set_mark(buffer_view, buffer_view->cursor.pos);
        }
        return true;
    }
    return false;
}

bool view_handle_mouse_click(View *view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, Render_State *render_state)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        return buffer_view_handle_mouse_click(&view->bv, frame_rect, mouse_canvas_pos, is_shift_pressed, render_state);
    }
    return false;
}

void frame_handle_mouse_click(Frame *frame, Vec_2 mouse_canvas_pos, Mouse_State *mouse_state, Render_State *render_state, bool is_shift_pressed, bool will_propagate_to_view)
{
    Rect resize_handle_rect = frame_get_resize_handle_rect(*frame, render_state);
    if (rect_p_intersect(mouse_canvas_pos, resize_handle_rect))
        mouse_state->resized_frame = frame;
    else if (will_propagate_to_view && view_handle_mouse_click(frame->view, frame->outer_rect, mouse_canvas_pos, is_shift_pressed, render_state))
        mouse_state->drag_in_view_frame = frame;
    else
        mouse_state->dragged_frame = frame;
}

void handle_mouse_click(GLFWwindow *window, Editor_State *state)
{
    Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
    bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    Frame *clicked_frame = frame_at_pos(mouse_canvas_pos, state);
    if (clicked_frame != NULL)
    {
        if (state->active_frame != clicked_frame)
        {
            frame_set_active(clicked_frame, state);
            frame_handle_mouse_click(clicked_frame, mouse_canvas_pos, &state->mouse_state, &state->render_state, is_shift_pressed, false);
        }
        else
        {
            frame_handle_mouse_click(clicked_frame, mouse_canvas_pos, &state->mouse_state, &state->render_state, is_shift_pressed, true);
        }
    }
}

void buffer_view_handle_mouse_release(Buffer_View *buffer_view)
{
    buffer_view___validate_mark(buffer_view);
}

void view_handle_mouse_release(View *view)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        buffer_view_handle_mouse_release(&view->bv);
    }
}

void handle_mouse_release(Mouse_State *mouse_state)
{
    mouse_state->resized_frame = NULL;
    mouse_state->dragged_frame = NULL;
    mouse_state->scrolled_frame = NULL;
    if (mouse_state->drag_in_view_frame)
    {
        view_handle_mouse_release(mouse_state->drag_in_view_frame->view);
        mouse_state->drag_in_view_frame = NULL;
    }
}

void handle_mouse_input(GLFWwindow *window, Editor_State *state)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        Vec_2 mouse_delta = get_mouse_delta(window, &state->mouse_state);
        Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(state);
        handle_mouse_click_drag(mouse_canvas_pos, mouse_delta, is_shift_pressed, &state->mouse_state, &state->render_state);
    }
}
