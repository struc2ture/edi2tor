#pragma once

#include <stdbool.h>

#include "editor.h"

void input_key_global(Editor_State *state, const Platform_Event *e);
void input_key_view(Editor_State *state, View *view, const Platform_Event *e);
void input_key_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e);

void buffer_view_handle_click_drag(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state);
void view_handle_click_drag(View *view, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state);
void view_handle_drag(View *view, Vec_2 drag_delta, Render_State *render_state);
void view_handle_resize(View *view, Vec_2 drag_delta, Render_State *render_state);
void handle_mouse_click_drag(Vec_2 mouse_canvas_pos, Vec_2 mouse_delta, bool is_shift_pressed, Mouse_State *mouse_state, Render_State *render_state);

bool buffer_view_handle_mouse_click(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state);
void view_handle_mouse_click(View *view, Vec_2 mouse_canvas_pos, Mouse_State *mouse_state, bool is_shift_pressed, bool should_propagate_inside, Render_State *render_state);
void handle_mouse_click(GLFWwindow *window, Editor_State *state);

void buffer_view_handle_mouse_release(Buffer_View *buffer_view);
void view_handle_mouse_release(View *view);
void handle_mouse_release(Mouse_State *mouse_state);

void handle_mouse_input(Editor_State *state);
