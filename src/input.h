#pragma once

#include <stdbool.h>

#include "editor.h"

void buffer_view_handle_key(Buffer_View *buffer_view, Frame *frame, GLFWwindow *window, Editor_State *state, int key, int action, int mods);
void view_handle_key(View *view, Frame *frame, GLFWwindow *window, Editor_State *state, int key, int action, int mods);
void handle_key_input(GLFWwindow *window, Editor_State *state, int key, int action, int mods);

void buffer_view_handle_click_drag(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state);
void view_handle_click_drag(View *view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state);
void frame_handle_drag(Frame *frame, Vec_2 drag_delta, Render_State *render_state);
void frame_handle_resize(Frame *frame, Vec_2 drag_delta, Render_State *render_state);
void handle_mouse_click_drag(Vec_2 mouse_canvas_pos, Vec_2 mouse_delta, bool is_shift_pressed, Mouse_State *mouse_state, Render_State *render_state);

bool buffer_view_handle_mouse_click(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state);
bool view_handle_mouse_click(View *view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, Render_State *render_state);
void frame_handle_mouse_click(Frame *frame, Vec_2 mouse_canvas_pos, Mouse_State *mouse_state, Render_State *render_state, bool is_shift_pressed, bool will_propagate_to_view);
void handle_mouse_click(GLFWwindow *window, Editor_State *state);

void buffer_view_handle_mouse_release(Buffer_View *buffer_view);
void view_handle_mouse_release(View *view);
void handle_mouse_release(Mouse_State *mouse_state);

void handle_mouse_input(GLFWwindow *window, Editor_State *state);
