#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <OpenGL/gl3.h>

#include "live_cube.h"

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

typedef float mat4[16]; // column-major

void mat4_identity(mat4 m) {
    memset(m, 0, sizeof(mat4));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4_perspective(mat4 m, float fov, float aspect, float znear, float zfar) {
    float tan_half = tanf(fov / 2.0f);
    memset(m, 0, sizeof(mat4));
    m[0] = 1.0f / (aspect * tan_half);
    m[5] = 1.0f / tan_half;
    m[10] = -(zfar + znear) / (zfar - znear);
    m[11] = -1.0f;
    m[14] = -(2.0f * zfar * znear) / (zfar - znear);
}

void mat4_translate(mat4 m, float x, float y, float z) {
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void mat4_rotate_y(mat4 m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    m[0] =  c;
    m[2] =  s;
    m[8] = -s;
    m[10] = c;
}

void mat4_rotate_x(mat4 m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    m[5]  =  c;
    m[6]  = -s;
    m[9]  =  s;
    m[10] =  c;
}

void mat4_mul(mat4 out, mat4 a, mat4 b) {
    mat4 res;
    for (int col = 0; col < 4; col++)
    for (int row = 0; row < 4; row++) {
        res[col*4 + row] = 0.0f;
        for (int i = 0; i < 4; i++)
            res[col*4 + row] += a[i*4 + row] * b[col*4 + i];
    }
    memcpy(out, res, sizeof(mat4));
}

void live_cube_init(User_State *state, float w, float h)
{
    const char *vs_src =
        "#version 410 core\n"
        "layout(location = 0) in vec3 aPos;\n"
        "layout(location = 1) in vec3 aColor;\n"
        "out vec3 Color;\n"
        "uniform mat4 u_mvp;\n"
        "void main() {\n"
        "  gl_Position = u_mvp * vec4(aPos, 1.0);\n"
        "  Color = aColor;\n"
        "}\n";

    const char *fs_src =
        "#version 410 core\n"
        "in vec3 Color;\n"
        "out vec4 FragColor;\n"
        "void main() {\n"
        "  FragColor = vec4(Color, 1.0);\n"
        "}\n";

    state->prog = gl_create_shader_program(vs_src, fs_src);

    float cube_verts[] = {
        // positions         // colors
        -1, -1, -1,  1, 0, 0,
        1, -1, -1,  0, 1, 0,
        1,  1, -1,  0, 0, 1,
        -1,  1, -1,  1, 1, 0,
        -1, -1,  1,  1, 0, 1,
        1, -1,  1,  0, 1, 1,
        1,  1,  1,  1, 1, 1,
        -1,  1,  1,  0, 0, 0,
    };

    unsigned char cube_indices[] = {
        0,1,2, 2,3,0,  // back
        4,5,6, 6,7,4,  // front
        0,4,7, 7,3,0,  // left
        1,5,6, 6,2,1,  // right
        3,2,6, 6,7,3,  // top
        0,1,5, 5,4,0   // bottom
    };

    glGenVertexArrays(1, &state->vao);
    glGenBuffers(1, &state->vbo);
    glGenBuffers(1, &state->ebo);

    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verts), cube_verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    state->w = w;
    state->h = h;
}

void live_cube_draw(User_State *state, float delta_time)
{
    glEnable(GL_DEPTH_TEST);

    glClearColor(0.4f, 0.5f, 0.6f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(state->prog);
    glBindVertexArray(state->vao);

    mat4 rot_x, rot_y, model, view, proj, mv, mvp;

    state->angle += delta_time;

    mat4_rotate_x(rot_x, state->angle * 0.7f);
    mat4_rotate_y(rot_y, state->angle);
    mat4_mul(model, rot_y, rot_x);
    mat4_translate(view, 0.0f, 0.0f, -5.0f); // simple "look at" from z = -5
    mat4_perspective(proj, 3.14f / 3.0f, state->w / state->h, 0.1f, 100.0f);

    mat4_mul(mv, view, model);
    mat4_mul(mvp, proj, mv);

    GLuint loc = glGetUniformLocation(state->prog, "u_mvp");
    glUniformMatrix4fv(loc, 1, GL_FALSE, mvp);

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, 0);
    glDisable(GL_DEPTH_TEST);
}

void live_cube_destroy(User_State *state)
{
    glDeleteBuffers(1, &state->vbo);
    glDeleteVertexArrays(1, &state->vao);
    glDeleteProgram(state->prog);
}
