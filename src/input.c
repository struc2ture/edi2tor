#include "input.h"

#include <stdbool.h>

#include <SDL3/sdl.h>

#include "actions.h"
#include "editor.h"
#include "util.h"

void input_key_global(Editor_State *state, const SDL_Event *e)
{
    const SDL_KeyboardEvent *ke = &e->key;
    bool will_propagate_to_view = true;
    if (ke->type == SDL_EVENT_KEY_DOWN)
    {
        if (ke->mod == SDL_KMOD_NONE)
        {
            switch (ke->key)
            {
                case SDLK_F1:
                {
                    action_run_unit_tests(state);
                } break;
                case SDLK_F2:
                {
                    action_change_working_dir(state);
                } break;
                case SDLK_F5:
                {
                    action_rebuild_live_scene(state);
                } break;
                case SDLK_F6:
                {
                    action_link_live_scene(state);
                } break;
                case SDLK_F12:
                {
                    action_debug_break(state);
                } break;
            }
        }
        else if (ke->mod == SDL_KMOD_SHIFT)
        {
            switch (ke->key)
            {
                case SDLK_F5:
                {
                    action_reset_live_scene(state);
                } break;
            }
        }
        else if (ke->mod == SDL_KMOD_GUI)
        {
            switch (ke->mod)
            {
                case SDLK_W:
                {
                    will_propagate_to_view = false;
                    action_destroy_active_view(state);
                } break;
                case SDLK_1:
                {
                    action_open_test_file1(state);
                } break;
                case SDLK_2:
                {
                    action_open_test_image(state);
                } break;
                case SDLK_3:
                {
                    action_open_test_live_scene(state);
                } break;
                case SDLK_O:
                {
                    action_prompt_open_file(state);
                } break;
                case SDLK_N:
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

void input_key_view(Editor_State *state, View *view, const SDL_Event *e)
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
            log_warning("input_key_view: Unhandled View kind: %d", view->kind);
        } break;
    }
}

void input_key_buffer_view(Editor_State *state, Buffer_View *buffer_view, const SDL_Event *e)
{
    const SDL_KeyboardEvent *ke = &e->key;
    if (ke->type == SDL_EVENT_KEY_DOWN)
    {
        if (ke->key == SDLK_LEFT ||
            ke->key == SDLK_RIGHT ||
            ke->key == SDLK_UP ||
            ke->key == SDLK_DOWN)
        {
            Cursor_Movement_Dir dir = CURSOR_MOVE_UP;
            switch (ke->key)
            {
                case SDLK_LEFT: dir = CURSOR_MOVE_LEFT; break;
                case SDLK_RIGHT: dir = CURSOR_MOVE_RIGHT; break;
                case SDLK_UP: dir = CURSOR_MOVE_UP; break;
                case SDLK_DOWN: dir = CURSOR_MOVE_DOWN; break;
            }
            action_buffer_view_move_cursor(state, buffer_view, dir, ke->mod & SDL_KMOD_SHIFT, ke->mod & SDL_KMOD_ALT, ke->mod & SDL_KMOD_GUI);
        }
        else if (ke->mod == SDL_KMOD_NONE)
        {
            switch (ke->key)
            {
                case SDLK_RETURN:
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

                case SDLK_BACKSPACE:
                {
                    action_buffer_view_backspace(state, buffer_view);
                } break;

                case SDLK_TAB:
                {
                    action_buffer_view_insert_indent(state, buffer_view);
                } break;
            }
        }
        else if (ke->mod == SDL_KMOD_GUI)
        {
            switch (ke->key)
            {
                case SDLK_LEFTBRACKET:
                {
                    action_buffer_view_decrease_indent_level(state, buffer_view);
                } break;
                case SDLK_RIGHTBRACKET:
                {
                    action_buffer_view_increase_indent_level(state, buffer_view);
                } break;
                case SDLK_C:
                {
                    action_buffer_view_copy_selected(state, buffer_view);
                } break;
                case SDLK_V:
                {
                    action_buffer_view_paste(state, buffer_view);
                } break;
                case SDLK_X:
                {
                    action_buffer_view_delete_current_line(state, buffer_view);
                } break;
                case SDLK_R:
                {
                    if (buffer_view->buffer->kind == BUFFER_KIND_FILE)
                    {
                        action_buffer_view_reload_file(state, buffer_view);
                    }
                } break;
                case SDLK_S:
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
                case SDLK_EQUALS:
                {
                    action_buffer_view_change_zoom(state, buffer_view, 0.25f);
                } break;
                case SDLK_MINUS:
                {
                    action_buffer_view_change_zoom(state, buffer_view, -0.25f);
                } break;
                case SDLK_G:
                {
                    action_buffer_view_prompt_go_to_line(state, buffer_view);
                } break;
                case SDLK_F:
                {
                    action_buffer_view_prompt_search_next(state, buffer_view);
                } break;
                case SDLK_SEMICOLON:
                {
                    action_buffer_view_whitespace_cleanup(state, buffer_view);
                } break;
            }
        }
        else if (ke->mod == (SDL_KMOD_GUI | SDL_KMOD_SHIFT))
        {
            switch (ke->key)
            {
                case SDLK_S:
                {
                    action_buffer_view_prompt_save_file_as(state, buffer_view);
                } break;

                case SDLK_F:
                {
                    action_buffer_view_repeat_search(state, buffer_view);
                } break;
            }
        }
    }
}

void input_mouse_update(Editor_State *state)
{
    SDL_MouseButtonFlags mouse_button_flags = SDL_GetMouseState(&state->mouse_state.pos.x, &state->mouse_state.pos.y);
    
    if (mouse_button_flags == SDL_BUTTON_MASK(SDL_BUTTON_LEFT))
    {
        input_mouse_drag_global(state);
    }

    state->mouse_state.prev_mouse_pos = state->mouse_state.pos;

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
    Mouse_State *mouse_state = &state->mouse_state;
    if (mouse_state->inner_drag_view)
    {
        input_mouse_drag_view(state, mouse_state->inner_drag_view);
    }
    else if (mouse_state->dragged_view)
    {
        Rect new_rect = mouse_state->dragged_view->outer_rect;
        new_rect.x += mouse_state->pos.x - mouse_state->prev_mouse_pos.x;
        new_rect.y += mouse_state->pos.y - mouse_state->prev_mouse_pos.y;
        view_set_rect(mouse_state->dragged_view, new_rect, &state->render_state);
    }
    else if (mouse_state->resized_view)
    {
        Rect new_rect = mouse_state->resized_view->outer_rect;
        new_rect.w += mouse_state->pos.x - mouse_state->prev_mouse_pos.x;
        new_rect.h += mouse_state->pos.y - mouse_state->prev_mouse_pos.y;
        view_set_rect(mouse_state->resized_view, new_rect, &state->render_state);
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
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);
    buffer_view_set_cursor_to_pixel_position(buffer_view, mouse_canvas_pos, &state->render_state);
}

void input_mouse_click_global(Editor_State *state)
{
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);
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
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);
    SDL_Keymod mods = SDL_GetModState();
    bool is_shift_pressed = (mods & SDL_KMOD_SHIFT);

    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, &state->render_state);
    if (rect_p_intersect(mouse_canvas_pos, text_area_rect))
    {
        if (is_shift_pressed)
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

void input_mouse_scroll_global(Editor_State *state, const SDL_Event *e)
{
    if (state->mouse_state.scroll_timeout <= 0.0f)
    {
        Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);
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
        state->canvas_viewport.rect.x -= e->wheel.x * SCROLL_SENS;
        state->canvas_viewport.rect.y -= e->wheel.y * SCROLL_SENS;
    }
}

bool input_mouse_scroll_view(Editor_State *state, View *view, const SDL_Event *e)
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

bool input_mouse_scroll_buffer_view(Editor_State *state, Buffer_View *buffer_view, const SDL_Event *e)
{
    buffer_view->viewport.rect.x -= e->wheel.x * SCROLL_SENS;
    buffer_view->viewport.rect.y -= e->wheel.y * SCROLL_SENS;

    if (buffer_view->viewport.rect.x < 0.0f) buffer_view->viewport.rect.x = 0.0f;
    float buffer_max_x = 256 * get_font_line_height(state->render_state.font); // TODO: Determine max x coordinates based on longest line
    if (buffer_view->viewport.rect.x > buffer_max_x) buffer_view->viewport.rect.x = buffer_max_x;

    if (buffer_view->viewport.rect.y < 0.0f) buffer_view->viewport.rect.y = 0.0f;
    float buffer_max_y = (buffer_view->buffer->text_buffer.line_count - 1) * get_font_line_height(state->render_state.font);
    if (buffer_view->viewport.rect.y > buffer_max_y) buffer_view->viewport.rect.y = buffer_max_y;

    return true;
}
