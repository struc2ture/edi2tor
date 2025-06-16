#pragma once

#include <stdbool.h>

#include <SDL3/sdl.h>

#include "editor.h"

void input_key_global(Editor_State *state, const SDL_Event *e);
void input_key_view(Editor_State *state, View *view, const SDL_Event *e);
void input_key_buffer_view(Editor_State *state, Buffer_View *buffer_view, const SDL_Event *e);

void input_mouse_update(Editor_State *state);
void input_mouse_drag_global(Editor_State *state);
void input_mouse_drag_view(Editor_State *state, View *view);
void input_mouse_drag_buffer_view(Editor_State *state, Buffer_View *buffer_view);
void input_mouse_click_global(Editor_State *state);
bool input_mouse_click_view(Editor_State *state, View *view);
bool input_mouse_click_buffer_view(Editor_State *state, Buffer_View *buffer_view);
void input_mouse_release_global(Editor_State *state);
void input_mouse_release_view(Editor_State *state, View *view);
void input_mouse_release_buffer_view(Editor_State *state, Buffer_View *buffer_view);

void input_mouse_scroll_global(Editor_State *state, const SDL_Event *e);
bool input_mouse_scroll_view(Editor_State *state, View *view, const SDL_Event *e);
bool input_mouse_scroll_buffer_view(Editor_State *state, Buffer_View *buffer_view, const SDL_Event *e);
