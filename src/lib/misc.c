#include "misc.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <OpenGL/gl3.h>

#include "common.h"
#include "util.h"

bool gl_check_compile_success(GLuint shader, const char *src)
{
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
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
    if (!success)
    {
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

void gl_enable_scissor(Rect screen_rect, float dpi_scale, float window_h)
{
    glEnable(GL_SCISSOR_TEST);
    Rect scaled_rect = {
        .x = screen_rect.x * dpi_scale,
        .y = screen_rect.y * dpi_scale,
        .w = screen_rect.w * dpi_scale,
        .h = screen_rect.h * dpi_scale
    };
    float scaled_window_h = window_h * dpi_scale;
    GLint scissor_x = (GLint)floor(scaled_rect.x);
    float screen_rect_topdown_bottom_y = scaled_rect.y + scaled_rect.h;
    float screen_rect_bottomup_bottom_y = scaled_window_h - screen_rect_topdown_bottom_y;
    GLint scissor_y = (GLint)floor(screen_rect_bottomup_bottom_y);
    GLsizei scissor_w = (GLsizei)(ceil(scaled_rect.x + scaled_rect.w) - scissor_x);
    GLsizei scissor_h = (GLsizei)(ceil(screen_rect_bottomup_bottom_y + scaled_rect.h) - scissor_y);
    glScissor(scissor_x, scissor_y, scissor_w, scissor_h);
}

void gl_disable_scissor()
{
    glDisable(GL_SCISSOR_TEST);
}

Gl_Framebuffer gl_create_framebuffer(int w, int h)
{
    Gl_Framebuffer framebuffer;
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

void gl_destroy_framebuffer(Gl_Framebuffer *framebuffer)
{
    glDeleteRenderbuffers(1, &framebuffer->depth_rb);
    glDeleteTextures(1, &framebuffer->tex);
    glDeleteFramebuffers(1, &framebuffer->fbo);
    framebuffer->depth_rb = 0;
    framebuffer->tex = 0;
    framebuffer->fbo = 0;
}

void mat_stack_push(Mat_Stack *s)
{
    if (s->capacity == 0)
    {
        s->capacity = 8;
        s->mm = xrealloc(s->mm, s->capacity * sizeof(s->mm[0]));
    }

    if (s->size >= s->capacity)
    {
        s->capacity *= 2;
        s->mm = xrealloc(s->mm, s->capacity * sizeof(s->mm[0]));
    }

    if (s->size > 0)
    {
        s->mm[s->size] = s->mm[s->size - 1];
    }
    else
    {
        s->mm[s->size] = mat4_identity();
    }

    s->size++;
}

Mat_4 mat_stack_pop(Mat_Stack *s)
{
    bassert(s->size > 0);
    s->size--;
    return s->mm[s->size];
}

Mat_4 mat_stack_peek(Mat_Stack *s)
{
    return s->mm[s->size - 1];
}

void mat_stack_mul_r(Mat_Stack *s, Mat_4 m)
{
    bassert(s->size > 0);
    s->mm[s->size - 1] = mat4_mul(s->mm[s->size - 1], m);
}

char *strf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    size_t size = vsnprintf(NULL, 0, fmt, args) + 1;
    va_end(args);
    char *str = xmalloc(size);
    va_start(args, fmt);
    vsnprintf(str, size, fmt, args);
    va_end(args);
    return str;
}

char *path_get_file_name(const char *path)
{
    const char *name = path;
    const char *slash = strrchr(name, '/');
    if (slash != NULL) name = slash + 1;
    const char *dot = strrchr(name, '.');
    size_t name_len = dot - name;
    char *result = malloc(name_len + 1);
    snprintf(result, name_len + 1, "%.*s", (int)name_len, name);
    return result;
}
