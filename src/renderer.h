#pragma once

#include "common.h"
#include "color.h"
#include "editor.h"
#include "types.h"

void draw_quad(Rect q, Color c, const Render_State *render_state);
void draw_texture(GLuint texture, Rect q, Color c, const Render_State *render_state);
void draw_string(const char *str, Render_Font font, f32 x, f32 y, Color c, const Render_State *render_state);
void draw_grid(v2 offset, f32 spacing, const Render_State *render_state);
