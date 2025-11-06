#pragma once

#include <OpenGL/gl3.h>

#include "common.h"
#include "rect.h"
#include "types.h"

typedef struct Gl_Framebuffer {
    int w;
    int h;
    GLuint fbo;
    GLuint tex;
    GLuint depth_rb;
    GLuint prog;
    GLuint vao;
} Gl_Framebuffer;

struct Render_State;

bool gl_check_compile_success(GLuint shader, const char *src);
bool gl_check_link_success(GLuint prog);
GLuint gl_create_shader_program(const char *vs_src, const char *fs_src);
void gl_enable_scissor(Rect screen_rect, struct Render_State *render_state);
void gl_disable_scissor();
Gl_Framebuffer gl_create_framebuffer(int width, int height);
void gl_destroy_framebuffer(Gl_Framebuffer *framebuffer);

typedef struct Mat_Stack {
    m4 *mm;
    int size;
    int capacity;
} Mat_Stack;

void mat_stack_push(Mat_Stack *s);
m4 mat_stack_pop(Mat_Stack *s);
m4 mat_stack_peek(Mat_Stack *s);
void mat_stack_mul_r(Mat_Stack *s, m4 m);


char *strf(const char *fmt, ...);
char *path_get_file_name(const char *path);
