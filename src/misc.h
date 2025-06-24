#pragma once

#include <OpenGL/gl3.h>

#include "common.h"

typedef struct {
    int w;
    int h;
    GLuint fbo;
    GLuint tex;
    GLuint depth_rb;
    GLuint prog;
    GLuint vao;
} Gl_Framebuffer;

typedef struct Render_State Render_State;

bool gl_check_compile_success(GLuint shader, const char *src);
bool gl_check_link_success(GLuint prog);
GLuint gl_create_shader_program(const char *vs_src, const char *fs_src);
void gl_enable_scissor(Rect screen_rect, Render_State *render_state);
Gl_Framebuffer gl_create_framebuffer(int width, int height);
void gl_destroy_framebuffer(Gl_Framebuffer *framebuffer);
