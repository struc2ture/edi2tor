#include <stdbool.h>
#include <stdio.h>

#include <OpenGL/gl3.h>

#include "user_draw.h"

static bool gl_check_compile_success(GLuint shader, const char *src)
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

static bool gl_check_link_success(GLuint prog)
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

static GLuint gl_create_shader_program(const char *vs_src, const char *fs_src)
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

void user_init(User_State *state)
{
    const char *vs_src =
        "#version 410 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "void main() {\n"
        "  gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char *fs_src =
        "#version 410 core\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "  FragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    state->prog = gl_create_shader_program(vs_src, fs_src);

    float verts[] = {
        -0.5f, -0.5f,
        0.5f, -0.5f,
        0.0f,  0.5f
    };

    glGenVertexArrays(1, &state->vao);
    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void user_draw(User_State *state)
{
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(state->prog);
    glBindVertexArray(state->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
