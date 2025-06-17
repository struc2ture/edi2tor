#include "input.h"

#include <stdbool.h>

#include <GLFW/glfw3.h>

#include "actions.h"
#include "editor.h"
#include "platform_event.h"
#include "util.h"

void input_key_global(Editor_State *state, const Platform_Event *e)
{
    bool will_propagate_to_view = true;
    if (e->key.action == GLFW_PRESS)
    {
        if (e->key.mods == 0)
        {
            switch(e->key.key)
            {
                case GLFW_KEY_F1:
                {
                    action_run_unit_tests(state);
                } break;
                case GLFW_KEY_F2:
                {
                    action_change_working_dir(state);
                } break;
                case GLFW_KEY_F5:
                {
                    action_rebuild_live_scene(state);
                } break;
                case GLFW_KEY_F6:
                {
                    action_link_live_scene(state);
                } break;
                case GLFW_KEY_F12:
                {
                    action_debug_break(state);
                } break;
            }
        }
        else if (e->key.mods == GLFW_MOD_SHIFT)
        {
            switch(e->key.key)
            {
                case GLFW_KEY_F5:
                {
                    action_reset_live_scene(state);
                } break;
            }
        }
        else if (e->key.mods == GLFW_MOD_SUPER)
        {
            switch(e->key.key)
            {
                case GLFW_KEY_W:
                {
                    will_propagate_to_view = false;
                    action_destroy_active_view(state);
                } break;
                case GLFW_KEY_1:
                {
                    action_open_test_file1(state);
                } break;
                case GLFW_KEY_2:
                {
                    action_open_test_image(state);
                } break;
                case GLFW_KEY_3:
                {
                    action_open_test_live_scene(state);
                } break;
                case GLFW_KEY_O:
                {
                    action_prompt_open_file(state);
                } break;
                case GLFW_KEY_N:
                {
                    action_prompt_new_file(state);
                } break;
            }
        }
    }

    if (will_propagate_to_view && state->active_view)
    {
        input_key_view(state, state->active_view, e);
    }
}
void input_key_view(Editor_State *state, View *view, const Platform_Event *e)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            input_key_buffer_view(state, &view->bv, e);
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

void input_key_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e)
{
    if (e->key.action == GLFW_PRESS || e->key.action == GLFW_REPEAT)
    {
        if (e->key.mods == 0)
        {
            switch(e->key.key)
            {
                case GLFW_KEY_LEFT:
                case GLFW_KEY_RIGHT:
                case GLFW_KEY_UP:
                case GLFW_KEY_DOWN:
                {
                    Cursor_Movement_Dir dir = CURSOR_MOVE_UP;
                    switch (e->key.key)
                    {
                        case GLFW_KEY_LEFT: dir = CURSOR_MOVE_LEFT; break;
                        case GLFW_KEY_RIGHT: dir = CURSOR_MOVE_RIGHT; break;
                        case GLFW_KEY_UP: dir = CURSOR_MOVE_UP; break;
                        case GLFW_KEY_DOWN: dir = CURSOR_MOVE_DOWN; break;
                    }
                    action_buffer_view_move_cursor(state, buffer_view, dir, e->key.mods & GLFW_MOD_SHIFT, e->key.mods & GLFW_MOD_ALT, e->key.mods & GLFW_MOD_SUPER);
                } break;
                case GLFW_KEY_ENTER:
                {
                    switch (buffer_view->buffer->kind)
                    {
                        case BUFFER_KIND_PROMPT:
                        {
                            action_buffer_view_prompt_submit(state, buffer_view);
                        } break;

                        default:
                        {
                            action_buffer_view_input_char(state, buffer_view, '\n');
                        } break;
                    }
                } break;
                case GLFW_KEY_BACKSPACE:
                {
                    action_buffer_view_backspace(state, buffer_view);
                } break;
                case GLFW_KEY_TAB:
                {
                    action_buffer_view_insert_indent(state, buffer_view);
                } break;
            }
        }
        else if (e->key.mods == GLFW_MOD_SUPER)
        {
            switch(e->key.key)
            {
                case GLFW_KEY_LEFT_BRACKET:
                {
                    action_buffer_view_decrease_indent_level(state, buffer_view);
                } break;
                case GLFW_KEY_RIGHT_BRACKET:
                {
                    action_buffer_view_increase_indent_level(state, buffer_view);
                } break;
                case GLFW_KEY_C:
                {
                    action_buffer_view_copy_selected(state, buffer_view);
                } break;
                case GLFW_KEY_V:
                {
                    action_buffer_view_paste(state, buffer_view);
                } break;
                case GLFW_KEY_X:
                {
                    action_buffer_view_delete_current_line(state, buffer_view);
                } break;
                case GLFW_KEY_R:
                {
                    if (buffer_view->buffer->kind == BUFFER_KIND_FILE)
                    {
                        action_buffer_view_reload_file(state, buffer_view);
                    }
                } break;
                case GLFW_KEY_S:
                {
                    if (buffer_view->buffer->kind == BUFFER_KIND_FILE)
                    {
                        if (buffer_view->buffer->file.info.path == NULL)
                        {
                            action_buffer_view_prompt_save_file_as(state, buffer_view);
                        }
                        else
                        {
                            action_buffer_view_save_file(state, buffer_view);
                        }
                    }
                } break;
                case GLFW_KEY_EQUAL:
                {
                    action_buffer_view_change_zoom(state, buffer_view, 0.25f);
                } break;
                case GLFW_KEY_MINUS:
                {
                    action_buffer_view_change_zoom(state, buffer_view, -0.25f);
                } break;
                case GLFW_KEY_G:
                {
                    action_buffer_view_prompt_go_to_line(state, buffer_view);
                } break;
                case GLFW_KEY_F:
                {
                    action_buffer_view_prompt_search_next(state, buffer_view);
                } break;
                case GLFW_KEY_SEMICOLON:
                {
                    action_buffer_view_whitespace_cleanup(state, buffer_view);
                } break;
            }
        }
        else if (e->key.mods == (GLFW_MOD_SUPER | GLFW_MOD_SHIFT))
        {
            switch(e->key.key)
            {
                case GLFW_KEY_S:
                {
                    action_buffer_view_prompt_save_file_as(state, buffer_view);
                } break;
                case GLFW_KEY_F:
                {
                    action_buffer_view_repeat_search(state, buffer_view);
                } break;
            }
        }
    }
}

void buffer_view_handle_click_drag(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    (void)is_shift_pressed;
    buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, render_state);
}

void view_handle_click_drag(View *view, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        buffer_view_handle_click_drag(&view->bv, mouse_canvas_pos, is_shift_pressed, render_state);
    }
}

void view_handle_drag(View *view, Vec_2 drag_delta, Render_State *render_state)
{
    Rect new_rect = view->outer_rect;
    new_rect.x += drag_delta.x;
    new_rect.y += drag_delta.y;
    view_set_rect(view, new_rect, render_state);
}

void view_handle_resize(View *view, Vec_2 drag_delta, Render_State *render_state)
{
    Rect new_rect = view->outer_rect;
    new_rect.w += drag_delta.x;
    new_rect.h += drag_delta.y;
    view_set_rect(view, new_rect, render_state);
}

void handle_mouse_click_drag(Vec_2 mouse_canvas_pos, Vec_2 mouse_delta, bool is_shift_pressed, Mouse_State *mouse_state, Render_State *render_state)
{
    if (mouse_state->inner_drag_view)
    {
        view_handle_click_drag(mouse_state->inner_drag_view, mouse_canvas_pos, is_shift_pressed, render_state);
    }
    else if (mouse_state->dragged_view)
    {
        view_handle_drag(mouse_state->dragged_view, mouse_delta, render_state);
    }
    else if (mouse_state->resized_view)
    {
        view_handle_resize(mouse_state->resized_view, mouse_delta, render_state);
    }
}

bool buffer_view_handle_mouse_click(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, render_state);
    if (rect_p_intersect(mouse_canvas_pos, text_area_rect))
    {
        if (is_shift_pressed)
        {
            if (!buffer_view->mark.active)
                buffer_view_set_mark(buffer_view, buffer_view->cursor.pos);

            buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, render_state);

            buffer_view_validate_mark(buffer_view);
        }
        else
        {
            buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, render_state);
            buffer_view_set_mark(buffer_view, buffer_view->cursor.pos);
        }
        return true;
    }
    return false;
}

void view_handle_mouse_click(View *view, Vec_2 mouse_canvas_pos, Mouse_State *mouse_state, bool is_shift_pressed, bool should_propagate_inside, Render_State *render_state)
{
    Rect resize_handle_rect = view_get_resize_handle_rect(view, render_state);
    if (rect_p_intersect(mouse_canvas_pos, resize_handle_rect))
    {
        mouse_state->resized_view = view;
        return;
    }
    else if (should_propagate_inside)
    {
        switch (view->kind)
        {
            case VIEW_KIND_BUFFER:
            {
                if (buffer_view_handle_mouse_click(&view->bv, mouse_canvas_pos, is_shift_pressed, render_state))
                {
                    mouse_state->inner_drag_view = view;
                    return;
                }
            } break;

            default:
            {
                log_warning("view_handle_mouse_click: Unhandled View kind: %d", view->kind);
            } break;
        }
    }
    mouse_state->dragged_view = view;
}

void handle_mouse_click(GLFWwindow *window, Editor_State *state)
{
    Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
    bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    View *clicked_view = view_at_pos(mouse_canvas_pos, state);
    if (clicked_view != NULL)
    {
        if (state->active_view != clicked_view)
        {
            view_set_active(clicked_view, state);
            view_handle_mouse_click(clicked_view, mouse_canvas_pos, &state->mouse_state, is_shift_pressed, false, &state->render_state);
        }
        else
        {
            view_handle_mouse_click(clicked_view, mouse_canvas_pos, &state->mouse_state, is_shift_pressed, true, &state->render_state);
        }
    }
}

void buffer_view_handle_mouse_release(Buffer_View *buffer_view)
{
    buffer_view_validate_mark(buffer_view);
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
    mouse_state->resized_view = NULL;
    mouse_state->dragged_view = NULL;
    mouse_state->scrolled_view = NULL;
    if (mouse_state->inner_drag_view)
    {
        view_handle_mouse_release(mouse_state->inner_drag_view);
        mouse_state->inner_drag_view = NULL;
    }
}

void handle_mouse_input(Editor_State *state)
{
    if (glfwGetMouseButton(state->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        bool is_shift_pressed = glfwGetKey(state->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(state->window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        Vec_2 mouse_delta = get_mouse_delta(state->window, &state->mouse_state);
        Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(state);
        handle_mouse_click_drag(mouse_canvas_pos, mouse_delta, is_shift_pressed, &state->mouse_state, &state->render_state);
    }
}
