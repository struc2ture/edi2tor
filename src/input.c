#include "input.h"

#include "common.h"

#include <GLFW/glfw3.h>

#include "actions.h"
#include "editor.h"
#include "glfw_helpers.h"
#include "platform_types.h"

void input_char_global(Editor_State *state, const Platform_Event *e)
{
    if (state->active_view != NULL)
    {
        input_char_view(state, state->active_view, e);
    }
}

void input_char_view(Editor_State *state, View *view, const Platform_Event *e)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            action_buffer_view_input_char(state, &state->active_view->bv, (char)e->character.codepoint);
        } break;

        case VIEW_KIND_IMAGE:
        {
            // Nothing
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            view->lsv.live_scene->dylib.on_platform_event(view->lsv.live_scene->state, e);
        } break;
    }
}

void input_key_global(Editor_State *state, const Platform_Event *e)
{
    bool will_propagate_to_view = true;
    View *prev_active_view = state->active_view;
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
                    action_run_scratch(state);
                } break;
                case GLFW_KEY_F10:
                {
                    action_live_scene_toggle_capture_input(state);
                } break;
                case GLFW_KEY_F12:
                {
                    action_debug_break(state);
                } break;
            }
        }
        else if (e->key.mods == GLFW_MOD_SHIFT)
        {
            switch (e->key.key)
            {
                case GLFW_KEY_F5:
                {
                    action_reset_scratch(state);
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
        else if (e->key.mods == (GLFW_MOD_SHIFT | GLFW_MOD_SUPER))
        {
            switch (e->key.key)
            {
                case GLFW_KEY_F5:
                {
                    action_save_workspace(state);
                } break;

                case GLFW_KEY_F9:
                {
                    action_reload_workspace(state);
                } break;
            }
        }
    }

    if (will_propagate_to_view && prev_active_view)
    {
        input_key_view(state, prev_active_view, e);
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
            view->lsv.live_scene->dylib.on_platform_event(view->lsv.live_scene->state, e);
        } break;
    }
}

void input_key_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e)
{
    if (e->key.action == GLFW_PRESS || e->key.action == GLFW_REPEAT)
    {
        if (e->key.key == GLFW_KEY_LEFT ||
            e->key.key == GLFW_KEY_RIGHT ||
            e->key.key == GLFW_KEY_UP ||
            e->key.key == GLFW_KEY_DOWN)
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
        }

        // Enter and backspace ignore shift modifier
        else if ((e->key.key == GLFW_KEY_ENTER || e->key.key == GLFW_KEY_BACKSPACE) &&
            (e->key.mods == 0 || e->key.mods == GLFW_MOD_SHIFT))
        {
            switch(e->key.key)
            {
                case GLFW_KEY_ENTER:
                {
                    if (buffer_view->buffer->prompt_context.kind == PROMPT_NONE)
                    {
                        action_buffer_view_input_char(state, buffer_view, '\n');
                    }
                    else
                    {
                        action_buffer_view_prompt_submit(state, buffer_view);
                    }
                } break;
                case GLFW_KEY_BACKSPACE:
                {
                    action_buffer_view_backspace(state, buffer_view);
                } break;
            }
        }

        else if (e->key.mods == 0)
        {
            switch(e->key.key)
            {
                case GLFW_KEY_TAB:
                {
                    action_buffer_view_insert_indent(state, buffer_view);
                } break;
                case GLFW_KEY_F3:
                {
                    action_buffer_view_view_history(state, buffer_view);
                } break;
            }
        }

        else if (e->key.mods == GLFW_MOD_ALT)
        {
            switch (e->key.key)
            {
                case GLFW_KEY_BACKSPACE:
                {
                    action_buffer_view_backspace_word(state, buffer_view);
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
                    action_buffer_view_cut_selected(state, buffer_view);
                } break;
                case GLFW_KEY_R:
                {
                    action_buffer_view_reload_file(state, buffer_view);
                } break;
                case GLFW_KEY_S:
                {
                    action_buffer_view_save_file(state, buffer_view);
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
                case GLFW_KEY_Z:
                {
                    action_buffer_view_undo_command(state, buffer_view);
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

void input_mouse_update(Editor_State *state, float delta_time)
{
    if (state->mouse_state.scroll_timeout > 0.0f)
    {
        state->mouse_state.scroll_timeout -= delta_time;
    }
    else
    {
        state->mouse_state.scrolled_view = NULL;
    }
}

void input_mouse_motion_global(Editor_State *state, const Platform_Event *e)
{
    Mouse_State *m_state = &state->mouse_state;
    m_state->pos = e->mouse_motion.pos;
    if (m_state->canvas_dragged)
    {
        state->canvas_viewport.rect.x -= e->mouse_motion.delta.x;
        state->canvas_viewport.rect.y -= e->mouse_motion.delta.y;
    }
    else if (m_state->dragged_view)
    {
        Rect new_rect = m_state->dragged_view->outer_rect;
        new_rect.x += e->mouse_motion.delta.x;
        new_rect.y += e->mouse_motion.delta.y;
        view_set_rect(m_state->dragged_view, new_rect, &state->render_state);
    }
    else if (m_state->resized_view)
    {
        Rect new_rect = m_state->resized_view->outer_rect;
        new_rect.w += e->mouse_motion.delta.x;
        new_rect.h += e->mouse_motion.delta.y;
        view_set_rect(m_state->resized_view, new_rect, &state->render_state);
    }
    else if (state->active_view)
    {
        input_mouse_motion_view(state, state->active_view, e);
    }
}

void input_mouse_motion_view(Editor_State *state, View *view, const Platform_Event *e)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            input_mouse_motion_buffer_view(state, &view->bv, e);
        } break;

        case VIEW_KIND_IMAGE:
        {
            // Nothing
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            Platform_Event adjusted_event = input__adjust_mouse_event_for_live_scene_view(state, &view->lsv, e);
            view->lsv.live_scene->dylib.on_platform_event(view->lsv.live_scene->state, &adjusted_event);
        } break;
    }
}

void input_mouse_motion_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e)
{
    if (buffer_view->is_mouse_drag)
    {
        v2 mouse_canvas_pos = screen_pos_to_canvas_pos(e->mouse_motion.pos, state->canvas_viewport);
        buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, &state->render_state);
    }
}

void input_mouse_button_global(Editor_State *state, const Platform_Event *e)
{
    if (e->mouse_button.button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (e->mouse_button.action == GLFW_PRESS)
        {
            if (glfwh_is_super_pressed(state->window))
            {
                state->mouse_state.canvas_dragged = true;
            }
            else
            {
                v2 mouse_canvas_pos = screen_pos_to_canvas_pos(e->mouse_button.pos, state->canvas_viewport);
                View *clicked_view = view_at_pos(mouse_canvas_pos, state);
                if (clicked_view != NULL)
                {
                    bool will_propagate_to_inner_view = true;

                    // 1. Check if active view is being switched
                    if (state->active_view != clicked_view)
                    {
                        view_set_active(clicked_view, state);
                        will_propagate_to_inner_view = false;
                    }

                    bool found_target = false;

                    // 2. Check if view is being resized
                    Rect resize_handle_rect = view_get_resize_handle_rect(clicked_view, &state->render_state);
                    if (rect_contains_p(mouse_canvas_pos, resize_handle_rect))
                    {
                        state->mouse_state.resized_view = clicked_view;
                        will_propagate_to_inner_view = false;
                        found_target = true;
                    }

                    // 3. If active view wasn't just switched, and view isn't being resized, propagate click to inner view
                    if (will_propagate_to_inner_view)
                    {
                        Rect inner_view_rect = view_get_inner_rect(clicked_view, &state->render_state);
                        if (rect_contains_p(mouse_canvas_pos, inner_view_rect))
                        {
                            input_mouse_button_view(state, state->active_view, e);
                            found_target = true;
                        }
                    }

                    // 4. If view is not being resized, and click didn't propagate to inner view, view is being dragged
                    if (!found_target)
                    {
                        state->mouse_state.dragged_view = clicked_view;
                    }
                }
            }
        }
        else if (e->mouse_button.action == GLFW_RELEASE)
        {
            // Only pass release event to inner view if it doesn't follow canvas-level actions
            if (state->mouse_state.resized_view || state->mouse_state.dragged_view || state->mouse_state.canvas_dragged)
            {
                state->mouse_state.resized_view = NULL;
                state->mouse_state.dragged_view = NULL;
                state->mouse_state.canvas_dragged = false;
            }
            else if (state->active_view)
            {
                // I think it's ok to send release event to active view, because only mouse press can switch active view, so nothing could've switched it before GLFW_RELEASE is sent.
                input_mouse_button_view(state, state->active_view, e);
            }
        }
    }
    else if (e->mouse_button.button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (e->mouse_button.action == GLFW_PRESS)
        {
            state->mouse_state.canvas_dragged = true;
        }
        else if (e->mouse_button.action == GLFW_RELEASE)
        {
            state->mouse_state.canvas_dragged = false;
        }
    }
}

bool input_mouse_button_view(Editor_State *state, View *view, const Platform_Event *e)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            input_mouse_button_buffer_view(state, &view->bv, e);
        } break;

        case VIEW_KIND_IMAGE:
        {
            // Nothing
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            Platform_Event adjusted_event = input__adjust_mouse_event_for_live_scene_view(state, &view->lsv, e);
            view->lsv.live_scene->dylib.on_platform_event(view->lsv.live_scene->state, &adjusted_event);
        } break;
    }
    return false;
}

void input_mouse_button_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e)
{
    if (e->mouse_button.action == GLFW_PRESS)
    {
        v2 mouse_canvas_pos = screen_pos_to_canvas_pos(e->mouse_button.pos, state->canvas_viewport);
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
        buffer_view->is_mouse_drag = true;
    }
    else if (e->mouse_button.action == GLFW_RELEASE)
    {
        buffer_view_validate_mark(buffer_view);
        buffer_view->is_mouse_drag = false;
    }
}

void input_mouse_scroll_global(Editor_State *state, const Platform_Event *e)
{
    if (state->mouse_state.scroll_timeout <= 0.0f)
    {
        v2 mouse_canvas_pos = screen_pos_to_canvas_pos(e->mouse_scroll.pos, state->canvas_viewport);
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
        state->canvas_viewport.rect.x -= e->mouse_scroll.scroll.x * SCROLL_SENS;
        state->canvas_viewport.rect.y -= e->mouse_scroll.scroll.y * SCROLL_SENS;
    }
}

bool input_mouse_scroll_view(Editor_State *state, View *view, const Platform_Event *e)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            input_mouse_scroll_buffer_view(state, &view->bv, e);
            return true;
        } break;

        case VIEW_KIND_IMAGE:
        {
            // Nothing
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            Platform_Event adjusted_event = input__adjust_mouse_event_for_live_scene_view(state, &view->lsv, e);
            view->lsv.live_scene->dylib.on_platform_event(view->lsv.live_scene->state, &adjusted_event);
            return true;
        } break;
    }
    return false;
}

void input_mouse_scroll_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e)
{
    buffer_view->viewport.rect.x -= e->mouse_scroll.scroll.x * SCROLL_SENS;
    buffer_view->viewport.rect.y -= e->mouse_scroll.scroll.y * SCROLL_SENS;

    if (buffer_view->viewport.rect.x < 0.0f) buffer_view->viewport.rect.x = 0.0f;
    float buffer_max_x = 256 * get_font_line_height(state->render_state.font); // TODO: Determine max x coordinates based on longest line
    if (buffer_view->viewport.rect.x > buffer_max_x) buffer_view->viewport.rect.x = buffer_max_x;

    if (buffer_view->viewport.rect.y < 0.0f) buffer_view->viewport.rect.y = 0.0f;
    float buffer_max_y = (buffer_view->buffer->text_buffer.line_count - 1) * get_font_line_height(state->render_state.font);
    if (buffer_view->viewport.rect.y > buffer_max_y) buffer_view->viewport.rect.y = buffer_max_y;
}

Platform_Event input__adjust_mouse_event_for_live_scene_view(Editor_State *state, Live_Scene_View *lsv, const Platform_Event *e)
{
    Platform_Event adjusted_event = *e;
    v2 offset = canvas_pos_to_screen_pos(V2(lsv->framebuffer_rect.x, lsv->framebuffer_rect.y), state->canvas_viewport);
    switch (adjusted_event.kind)
    {
        case PLATFORM_EVENT_MOUSE_BUTTON:
        {
            adjusted_event.mouse_button.pos = vec2_sub(e->mouse_button.pos, offset);
        } break;

        case PLATFORM_EVENT_MOUSE_MOTION:
        {
            adjusted_event.mouse_motion.pos = vec2_sub(e->mouse_motion.pos, offset);
        } break;

        case PLATFORM_EVENT_MOUSE_SCROLL:
        {
            adjusted_event.mouse_scroll.pos = vec2_sub(e->mouse_scroll.pos, offset);
        } break;

        default: break;
    }
    return adjusted_event;
}
