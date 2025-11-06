#include "renderer.h"

#include "color.h"
#include "common.h"
#include "editor.h"
#include "types.h"

void draw_quad(Rect q, Color c, const Render_State *render_state)
{
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 0, 0, c));
    glUseProgram(render_state->quad_shader);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_texture(GLuint texture, Rect q, Color c, const Render_State *render_state)
{
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 1, 1, c));
    glUseProgram(render_state->tex_shader);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_string(const char *str, Render_Font font, f32 x, f32 y, Color c, const Render_State *render_state)
{
    // NOTE: Assumes glUseProgram(font_shader) is called outside
    y += font.ascent * font.i_dpi_scale;
    Vert_Buffer vert_buf = {0};
    while (*str)
    {
        if (*str >= 32)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font.char_data, font.atlas_w, font.atlas_h, *str-32, &x, &y ,&q, 1, font.i_dpi_scale);
            vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y0, q.s0, q.t0, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y1, q.s1, q.t1, c));
        }
        str++;
    }
    glBufferData(GL_ARRAY_BUFFER, VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW); // Orphan existing buffer to stay on the fast path on mac
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
}

void draw_grid(v2 offset, f32 spacing, const Render_State *render_state)
{
    glUseProgram(render_state->grid_shader);
    glUniform2f(render_state->grid_shader_resolution_loc, render_state->framebuffer_dim.x, render_state->framebuffer_dim.y);
    f32 scaled_offset_x = offset.x * render_state->dpi_scale;
    f32 scaled_offset_y = offset.y * render_state->dpi_scale;
    glUniform2f(render_state->grid_shader_offset_loc, scaled_offset_x, scaled_offset_y);
    glUniform1f(render_state->grid_shader_spacing_loc, spacing * render_state->dpi_scale);

    Rect q = {0, 0, render_state->window_dim.x, render_state->window_dim.y};
    Color c = {0};
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 0, 0, c));
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}
