#include "input.h"

#include "common.h"

#include <GLFW/glfw3.h>

#include "actions.h"
#include "editor.h"
#include "glfw_helpers.h"
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

// --------------------------------

void input_mouse_update(Editor_State *state)
{
    state->mouse_state.current_pos = glfwh_get_mouse_position(state->window);
    state->mouse_state.delta = vec2_sub(state->mouse_state.current_pos, state->mouse_state.prev_pos);
    state->mouse_state.prev_pos = state->mouse_state.current_pos;

    if (glfwGetMouseButton(state->window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        input_mouse_drag_global(state);
    }

    if (state->mouse_state.scroll_timeout > 0.0f)
    {
        state->mouse_state.scroll_timeout -= state->delta_time;
    }
    else
    {
        state->mouse_state.scrolled_view = NULL;
    }
}

void input_mouse_drag_global(Editor_State *state)
{
    Mouse_State *m_state = &state->mouse_state;
    if (m_state->inner_drag_view)
    {
        input_mouse_drag_view(state, m_state->inner_drag_view);
    }
    else if (m_state->dragged_view)
    {
        Rect new_rect = m_state->dragged_view->outer_rect;
        new_rect.x += m_state->delta.x;
        new_rect.y += m_state->delta.y;
        view_set_rect(m_state->dragged_view, new_rect, &state->render_state);
    }
    else if (m_state->resized_view)
    {
        Rect new_rect = m_state->resized_view->outer_rect;
        new_rect.w += m_state->delta.x;
        new_rect.h += m_state->delta.y;
        view_set_rect(m_state->resized_view, new_rect, &state->render_state);
    }
}

void input_mouse_drag_view(Editor_State *state, View *view)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            input_mouse_drag_buffer_view(state, &view->bv);
        } break;

        default:
        {
            log_warning("input_mouse_drag_view: Unhandled View kind: %d", view->kind);
        } break;
    }
}

void input_mouse_drag_buffer_view(Editor_State *state, Buffer_View *buffer_view)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.current_pos, state->canvas_viewport);
    buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, &state->render_state);
}

void input_mouse_click_global(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.current_pos, state->canvas_viewport);
    View *clicked_view = view_at_pos(mouse_canvas_pos, state);
    if (clicked_view != NULL)
    {
        if (state->active_view != clicked_view)
        {
            // Active view switched, don't pass click inside the view

            view_set_active(clicked_view, state);

            Rect resize_handle_rect = view_get_resize_handle_rect(clicked_view, &state->render_state);
            if (rect_p_intersect(mouse_canvas_pos, resize_handle_rect))
            {
                state->mouse_state.resized_view = clicked_view;
            }
            else
            {
                state->mouse_state.dragged_view = clicked_view;
            }
        }
        else
        {
            Rect resize_handle_rect = view_get_resize_handle_rect(clicked_view, &state->render_state);
            if (rect_p_intersect(mouse_canvas_pos, resize_handle_rect))
            {
                state->mouse_state.resized_view = clicked_view;
            }
            else
            {
                bool click_handled_inside_view = input_mouse_click_view(state, clicked_view);
                if (!click_handled_inside_view)
                {
                    state->mouse_state.dragged_view = clicked_view;
                }
            }
        }
    }
}

bool input_mouse_click_view(Editor_State *state, View *view)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            if (input_mouse_click_buffer_view(state, &view->bv))
            {
                state->mouse_state.inner_drag_view = view;
                return true;
            }
        } break;

        default:
        {
            log_warning("input_mouse_click_view: Unhandled View kind: %d", view->kind);
        } break;
    }
    return false;
}

bool input_mouse_click_buffer_view(Editor_State *state, Buffer_View *buffer_view)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.current_pos, state->canvas_viewport);
    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, &state->render_state);
    if (rect_p_intersect(mouse_canvas_pos, text_area_rect))
    {
        if (glfwh_is_shift_pressed(state->window))
        {
            if (!buffer_view->mark.active)
                buffer_view_set_mark(buffer_view, buffer_view->cursor.pos);

            buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, &state->render_state);

            buffer_view_validate_mark(buffer_view);
        }
        else
        {
            buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, &state->render_state);
            buffer_view_set_mark(buffer_view, buffer_view->cursor.pos);
        }
        return true;
    }
    return false;
}

void input_mouse_release_global(Editor_State *state)
{
    Mouse_State *mouse_state = &state->mouse_state;
    mouse_state->resized_view = NULL;
    mouse_state->dragged_view = NULL;
    mouse_state->scrolled_view = NULL;
    if (mouse_state->inner_drag_view)
    {
        input_mouse_release_view(state, mouse_state->inner_drag_view);
        mouse_state->inner_drag_view = NULL;
    }
}

void input_mouse_release_view(Editor_State *state, View *view)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            input_mouse_release_buffer_view(state, &view->bv);
        } break;

        default:
        {
            log_warning("input_mouse_release_view: Unhandled View kind: %d");
        } break;
    }
}

void input_mouse_release_buffer_view(Editor_State *state, Buffer_View *buffer_view)
{
    (void)state;
    buffer_view_validate_mark(buffer_view);
}

void input_mouse_scroll_global(Editor_State *state, const Platform_Event *e)
{
    if (state->mouse_state.scroll_timeout <= 0.0f)
    {
        Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.current_pos, state->canvas_viewport);
        state->mouse_state.scrolled_view = view_at_pos(mouse_canvas_pos, state);
    }
    state->mouse_state.scroll_timeout = SCROLL_TIMEOUT; // There's an ongoing scroll when this function is called, so reset the scroll timeout

    bool scroll_handled_by_view = false;
    if (state->mouse_state.scrolled_view)
    {
        scroll_handled_by_view = input_mouse_scroll_view(state, state->mouse_state.scrolled_view, e);
    }
    if (!scroll_handled_by_view)
    {
        state->canvas_viewport.rect.x -= e->scroll.x_offset * SCROLL_SENS;
        state->canvas_viewport.rect.y -= e->scroll.y_offset * SCROLL_SENS;
    }
}

bool input_mouse_scroll_view(Editor_State *state, View *view, const Platform_Event *e)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            return input_mouse_scroll_buffer_view(state, &view->bv, e);
        } break;

        default:
        {
            log_warning("input_mouse_scroll_view: Unhandled View kind: %d", view->kind);
        } break;
    }
    return false;
}

bool input_mouse_scroll_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e)
{
    buffer_view->viewport.rect.x -= e->scroll.x_offset * SCROLL_SENS;
    buffer_view->viewport.rect.y -= e->scroll.y_offset * SCROLL_SENS;

    if (buffer_view->viewport.rect.x < 0.0f) buffer_view->viewport.rect.x = 0.0f;
    float buffer_max_x = 256 * get_font_line_height(state->render_state.font); // TODO: Determine max x coordinates based on longest line
    if (buffer_view->viewport.rect.x > buffer_max_x) buffer_view->viewport.rect.x = buffer_max_x;

    if (buffer_view->viewport.rect.y < 0.0f) buffer_view->viewport.rect.y = 0.0f;
    float buffer_max_y = (buffer_view->buffer->text_buffer.line_count - 1) * get_font_line_height(state->render_state.font);
    if (buffer_view->viewport.rect.y > buffer_max_y) buffer_view->viewport.rect.y = buffer_max_y;

    return true;
}
