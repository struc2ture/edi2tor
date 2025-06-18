#pragma once

#include "editor.h"

void input_char_global(Editor_State *state, const Platform_Event *e);
void input_char_view(Editor_State *state, View *view, const Platform_Event *e);

void input_key_global(Editor_State *state, const Platform_Event *e);
void input_key_view(Editor_State *state, View *view, const Platform_Event *e);
void input_key_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e);

void input_mouse_update(Editor_State *state, float delta_time);

void input_mouse_motion_global(Editor_State *state, const Platform_Event *event);
void input_mouse_motion_view(Editor_State *state, View *view, const Platform_Event *event);
void input_mouse_motion_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e);

void input_mouse_button_global(Editor_State *state, const Platform_Event *e);
bool input_mouse_button_view(Editor_State *state, View *view, const Platform_Event *e);
void input_mouse_button_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e);

void input_mouse_scroll_global(Editor_State *state, const Platform_Event *e);
bool input_mouse_scroll_view(Editor_State *state, View *view, const Platform_Event *e);
void input_mouse_scroll_buffer_view(Editor_State *state, Buffer_View *buffer_view, const Platform_Event *e);

Platform_Event input__adjust_mouse_event_for_live_scene_view(Editor_State *state, Live_Scene_View *lsv, const Platform_Event *e);
