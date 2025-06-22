#pragma once

#include <OpenGL/gl3.h>
#include <stb_truetype.h>

#include "common.h"

#define VERT_MAX 4096

typedef struct {
    unsigned char r, g, b, a;
} Color;

typedef struct {
    float x, y;
    float u, v;
    Color c;
} Vert;

typedef struct {
    Vert verts[VERT_MAX];
    int vert_count;
} Vert_Buffer;

typedef struct {
    stbtt_bakedchar *char_data;
    int char_count;
    GLuint texture;
    float size;
    float ascent, descent, line_gap;
    int atlas_w, atlas_h;
    float i_dpi_scale;
} Render_Font;

typedef struct {
    GLuint texture;
    float width;
    float height;
    int channels;
} Image;

typedef struct {
    int w;
    int h;
    GLuint fbo;
    GLuint tex;
    GLuint depth_rb;
    GLuint prog;
    GLuint vao;
} Framebuffer;

typedef struct Render_State {
    GLuint main_shader;
    GLuint grid_shader;
    GLuint image_shader;
    GLuint framebuffer_shader;
    GLuint vao;
    GLuint vbo;
    GLuint mvp_ubo;
    GLuint main_shader_mvp_loc;
    GLuint grid_shader_mvp_loc;
    GLuint grid_shader_offset_loc;
    GLuint grid_shader_spacing_loc;
    GLuint grid_shader_resolution_loc;
    GLuint image_shader_mvp_loc;
    GLuint framebuffer_shader_mvp_loc;
    Vec_2 window_dim;
    Vec_2 framebuffer_dim;
    float dpi_scale;
    Render_Font font;
    GLuint white_texture;
    float buffer_view_line_num_col_width;
    float buffer_view_name_height;
    float buffer_view_padding;
    float buffer_view_resize_handle_radius;
    GLuint default_fbo;
} Render_State;

bool gl_check_compile_success(GLuint shader, const char *src);
bool gl_check_link_success(GLuint prog);
GLuint gl_create_shader_program(const char *vs_src, const char *fs_src);
void gl_enable_scissor(Rect screen_rect, Render_State *render_state);
Framebuffer gl_create_framebuffer(int w, int h);
void initialize_render_state(Render_State *render_state, float window_w, float window_h, float window_px_w, float window_px_h, GLuint fbo);

Vert draw_make_vert(float x, float y, float u, float v, Color color);
void draw_vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert);

void draw_quad(Rect q, const Color color);
void draw_textured_quad(GLuint texture, Rect q, const Color color, Render_State *render_state);

Render_Font draw_string_font_load(const char *path, float dpi_scale);
float draw_string_font_get_line_height(Render_Font font);
float draw_string_font_get_char_width(char c, Render_Font font);
Rect draw_string_font_get_string_rect(const char *str, Render_Font font, float x, float y);
Rect draw_string_font_get_string_range_rect(const char *str, Render_Font font, int start_char, int end_char);
Rect draw_string_font_get_string_char_rect(const char *str, Render_Font font, int char_i);
int draw_string_font_get_char_i_at_pos_in_string(const char *str, Render_Font font, float x);
void draw_string(const char *str, Render_Font font, float x, float y, Color color, Render_State *render_state);

void draw_grid(Vec_2 offset, struct Render_State *render_state);
