#include "draw.h"

#include <OpenGL/gl3.h>
#include <math.h>

#include "common.h"
#include "editor.h"
#include "shaders.h"
#include "util.h"

bool gl_check_compile_success(GLuint shader, const char *src)
{
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\nSource:\n", log);
        fprintf(stderr, "%s\n", src);
    }
    return (bool)success;
}

bool gl_check_link_success(GLuint prog)
{
    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
    }
    return (bool)success;
}

GLuint gl_create_shader_program(const char *vs_src, const char *fs_src)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, 0);
    glCompileShader(vs);
    gl_check_compile_success(vs, vs_src);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, 0);
    glCompileShader(fs);
    gl_check_compile_success(fs, fs_src);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    gl_check_link_success(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

void gl_enable_scissor(Rect screen_rect, Render_State *render_state)
{
    glEnable(GL_SCISSOR_TEST);
    Rect scaled_rect = {
        .x = screen_rect.x * render_state->dpi_scale,
        .y = screen_rect.y * render_state->dpi_scale,
        .w = screen_rect.w * render_state->dpi_scale,
        .h = screen_rect.h * render_state->dpi_scale
    };
    float scaled_window_h = render_state->window_dim.y * render_state->dpi_scale;
    GLint scissor_x = (GLint)floor(scaled_rect.x);
    float screen_rect_topdown_bottom_y = scaled_rect.y + scaled_rect.h;
    float screen_rect_bottomup_bottom_y = scaled_window_h - screen_rect_topdown_bottom_y;
    GLint scissor_y = (GLint)floor(screen_rect_bottomup_bottom_y);
    GLsizei scissor_w = (GLsizei)(ceil(scaled_rect.x + scaled_rect.w) - scissor_x);
    GLsizei scissor_h = (GLsizei)(ceil(screen_rect_bottomup_bottom_y + scaled_rect.h) - scissor_y);
    glScissor(scissor_x, scissor_y, scissor_w, scissor_h);
}

Framebuffer gl_create_framebuffer(int w, int h)
{
    Framebuffer framebuffer;
    framebuffer.w = w;
    framebuffer.h = h;

    glGenFramebuffers(1, &framebuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

    glGenTextures(1, &framebuffer.tex);
    glBindTexture(GL_TEXTURE_2D, framebuffer.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer.w, framebuffer.h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.tex, 0);

    glGenRenderbuffers(1, &framebuffer.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, framebuffer.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer.depth_rb);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        log_warning("gl_create_framebuffer: Failed to create framebuffer");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return framebuffer;
}

void initialize_render_state(Render_State *render_state, float window_w, float window_h, float window_px_w, float window_px_h, GLuint fbo)
{
    render_state->window_dim.x = window_w;
    render_state->window_dim.y = window_h;
    render_state->framebuffer_dim.x = window_px_w;
    render_state->framebuffer_dim.y = window_px_h;
    render_state->dpi_scale = render_state->framebuffer_dim.x / render_state->window_dim.x;

    render_state->default_fbo = fbo;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    render_state->main_shader = gl_create_shader_program(shader_main_vert_src, shader_main_frag_src);
    render_state->grid_shader = gl_create_shader_program(shader_main_vert_src, shader_grid_frag_src);
    render_state->image_shader = gl_create_shader_program(shader_main_vert_src, shader_image_frag_src);
    render_state->framebuffer_shader = gl_create_shader_program(shader_framebuffer_vert_src, shader_framebuffer_frag_src);

    render_state->grid_shader_offset_loc = glGetUniformLocation(render_state->grid_shader, "u_offset");
    render_state->grid_shader_spacing_loc = glGetUniformLocation(render_state->grid_shader, "u_spacing");
    render_state->grid_shader_resolution_loc = glGetUniformLocation(render_state->grid_shader, "u_resolution");

    glGenBuffers(1, &render_state->mvp_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, render_state->mvp_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Mat_4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    GLuint mvp_ubo_binding_point = 0;
    glBindBufferBase(GL_UNIFORM_BUFFER, mvp_ubo_binding_point, render_state->mvp_ubo);
    glUniformBlockBinding(render_state->main_shader, glGetUniformBlockIndex(render_state->main_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->grid_shader, glGetUniformBlockIndex(render_state->grid_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->image_shader, glGetUniformBlockIndex(render_state->image_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->framebuffer_shader, glGetUniformBlockIndex(render_state->framebuffer_shader, "Matrices"), mvp_ubo_binding_point);

    glGenVertexArrays(1, &render_state->vao);
    glGenBuffers(1, &render_state->vbo);
    glBindVertexArray(render_state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, render_state->vbo);
    glBufferData(GL_ARRAY_BUFFER,  VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (void *)offsetof(Vert, c));
    glEnableVertexAttribArray(2);

    glGenTextures(1, &render_state->white_texture);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
    unsigned char white_texture_bytes[] = { 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, white_texture_bytes);

    render_state->font = draw_string_font_load(FONT_PATH, render_state->dpi_scale);
    render_state->buffer_view_line_num_col_width = draw_string_font_get_string_rect("000", render_state->font, 0, 0).w;
    render_state->buffer_view_name_height = draw_string_font_get_line_height(render_state->font);
    render_state->buffer_view_padding = 6.0f;
    render_state->buffer_view_resize_handle_radius = 5.0f;
}

// ----------------------------------------------------------

Vert draw_make_vert(float x, float y, float u, float v, Color color)
{
    Vert vert = {x, y, u, v, color};
    return vert;
}

void draw_vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert)
{
    bassert(vert_buffer->vert_count < VERT_MAX);
    vert_buffer->verts[vert_buffer->vert_count++] = vert;
}

// ----------------------------------------

void draw_quad(Rect q, Color color)
{
    Vert_Buffer vert_buf = {0};
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x,       q.y,       0, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x,       q.y + q.h, 0, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x + q.w, q.y,       0, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x + q.w, q.y,       0, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x,       q.y + q.h, 0, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x + q.w, q.y + q.h, 0, 0, color));
    glBufferData(GL_ARRAY_BUFFER, VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW); // Orphan existing buffer to stay on the fast path on mac
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_textured_quad(GLuint texture, Rect q, Color color, Render_State *render_state)
{
    Vert_Buffer vert_buf = {0};
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x,       q.y,       0, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x,       q.y + q.h, 0, 1, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x + q.w, q.y,       1, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x + q.w, q.y,       1, 0, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x,       q.y + q.h, 0, 1, color));
    draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x + q.w, q.y + q.h, 1, 1, color));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBufferData(GL_ARRAY_BUFFER, VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW); // Orphan existing buffer to stay on the fast path on mac
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
}

// -----------------------------------------------

Render_Font draw_string_font_load(const char *path, float dpi_scale)
{
    Render_Font font = {0};

    FILE *f = fopen(path, "rb");
    if (!f) fatal("Failed to open file for reading at %s", path);
    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *file_bytes = xmalloc(file_size);
    fread(file_bytes, 1, file_size, f);

    void *atlas_bitmap = xcalloc(512 * 512);

    font.size = FONT_SIZE;
    font.atlas_w = 512;
    font.atlas_h = 512;
    font.char_count = 96;
    font.char_data = xcalloc(font.char_count * sizeof(font.char_data[0]));
    font.i_dpi_scale = 1 / dpi_scale;
    stbtt_BakeFontBitmap(file_bytes, 0, font.size * dpi_scale, atlas_bitmap, font.atlas_w, font.atlas_h, 32, font.char_count, font.char_data);
    stbtt_GetScaledFontVMetrics(file_bytes, 0, font.size * dpi_scale, &font.ascent, &font.descent, &font.line_gap);
    free(file_bytes);

    glGenTextures(1, &font.texture);
    glBindTexture(GL_TEXTURE_2D, font.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.atlas_w, font.atlas_h, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    free(atlas_bitmap);

    return font;
}

float draw_string_font_get_line_height(Render_Font font)
{
    float height = (font.ascent - font.descent) * font.i_dpi_scale;
    return height;
}

float draw_string_font_get_char_width(char c, Render_Font font)
{
    float char_width = font.char_data[c - 32].xadvance * font.i_dpi_scale;
    return char_width;
}

Rect draw_string_font_get_string_rect(const char *str, Render_Font font, float x, float y)
{
    Rect r;
    r.x = x;
    r.y = y;
    r.w = 0;
    r.h = draw_string_font_get_line_height(font);
    while (*str)
    {
        r.w += draw_string_font_get_char_width(*str, font);
        str++;
    }
    return r;
}

Rect draw_string_font_get_string_range_rect(const char *str, Render_Font font, int start_char, int end_char)
{
    bassert(start_char >= 0);
    bassert(start_char < end_char);
    Rect r;
    r.y = 0;
    r.h = draw_string_font_get_line_height(font);
    float x = 0, min_x = 0, max_x = 0;
    for (int i = 0; i < end_char && str[i]; i++)
    {
        if (i == start_char)
        {
            min_x = x;
        }

        char c = str[i];
        if (str[i + 1 == '\0'] && c == '\n')
        {
            c = ' ';
        }
        x += draw_string_font_get_char_width(c, font);
    }
    // If reached end_char index, x will be max_x (end_char itself is not included)
    // If reached null terminator, it will still be valid, highlighting the whole string.
    max_x = x;
    r.x = min_x;
    r.w = max_x - min_x;
    return r;
}

Rect draw_string_font_get_string_char_rect(const char *str, Render_Font font, int char_i)
{
    Rect r = {0};
    r.h = draw_string_font_get_line_height(font);
    float x = 0, min_x = 0, max_x = 0;
    int i = 0;
    bool found_max = false;
    while (*str)
    {
        char c = *str;
        if (c == '\n')
        {
            c = ' ';
        }
        x += draw_string_font_get_char_width(c, font);
        if (i == char_i)
        {
            max_x = x;
            found_max = true;
            break;
        }
        else
        {
            min_x = x;
        }
        str++;
        i++;
    }
    bassert(found_max);
    r.x = min_x;
    r.w = max_x - min_x;
    return r;
}

int draw_string_font_get_char_i_at_pos_in_string(const char *str, Render_Font font, float x)
{
    int char_i = 0;
    float str_x = 0;
    while (*str)
    {
        str_x += draw_string_font_get_char_width(*str, font);
        if (str_x > x)
        {
            return char_i;
        }
        char_i++;
        str++;
    }
    return char_i;
}

void draw_string(const char *str, Render_Font font, float x, float y, Color color, Render_State *render_state)
{
    y += font.ascent * font.i_dpi_scale;
    Vert_Buffer vert_buf = {0};
    while (*str)
    {
        if (*str >= 32)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font.char_data, font.atlas_w, font.atlas_h, *str-32, &x, &y ,&q, 1, font.i_dpi_scale);
            draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x0, q.y0, q.s0, q.t0, color));
            draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x0, q.y1, q.s0, q.t1, color));
            draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x1, q.y0, q.s1, q.t0, color));
            draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x1, q.y0, q.s1, q.t0, color));
            draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x0, q.y1, q.s0, q.t1, color));
            draw_vert_buffer_add_vert(&vert_buf, draw_make_vert(q.x1, q.y1, q.s1, q.t1, color));
        }
        str++;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font.texture);
    glBufferData(GL_ARRAY_BUFFER, VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW); // Orphan existing buffer to stay on the fast path on mac
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
}

// --------------------------------------------------------

void draw_grid(Vec_2 offset, Render_State *render_state)
{
    Mat_4 proj = mat4_proj_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1);

    glUniformMatrix4fv(render_state->grid_shader_mvp_loc, 1, GL_FALSE, proj.m);

    glUniform2f(render_state->grid_shader_resolution_loc, render_state->framebuffer_dim.x, render_state->framebuffer_dim.y);

    float scaled_offset_x = offset.x * render_state->dpi_scale;
    float scaled_offset_y = offset.y * render_state->dpi_scale;
    glUniform2f(render_state->grid_shader_offset_loc, scaled_offset_x, scaled_offset_y);

    const float grid_spacing = 50.0f;
    glUniform1f(render_state->grid_shader_spacing_loc, grid_spacing * render_state->dpi_scale);

    draw_quad((Rect){0, 0, render_state->window_dim.x, render_state->window_dim.y}, (Color){0});
}
