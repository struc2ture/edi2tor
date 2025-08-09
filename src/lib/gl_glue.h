#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include <OpenGL/gl3.h>
#include <stb_image.h>

#include "common.h"

struct Texture
{
    GLuint texture_id;
    int w, h, ch;
    GLenum internal_format, format;
};

struct Col4f
{
    float r, g, b, a;
};

struct Vert
{
    float x, y, z;
    float u, v;
    struct Col4f color;
};

#define VERT_MAX 4096
#define INDEX_MAX 8192
struct Vert_Buf
{
    struct Vert verts[VERT_MAX];
    int vert_count;

    uint32_t indices[INDEX_MAX];
    int index_count;

    GLuint vao, vbo, ebo;
};

static bool gl_check_compile_success(GLuint shader, const char *src)
{
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);

        fprintf(stderr, "Shader compile error:\n%s\n\nSource:\n", log);
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

static GLuint gl_create_basic_shader()
{
    const char *vs_src =
        "#version 330 core\n"
        "layout (location = 0) in vec3 aPos;\n"
        "layout (location = 1) in vec2 aTexCoord;\n"
        "layout (location = 2) in vec4 aColor;\n"
        "uniform mat4 u_mvp;\n"
        "out vec2 TexCoord;\n"
        "out vec4 Color;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(aPos, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "    Color = aColor;\n"
        "}\n";

    const char *fs_src =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "uniform sampler2D u_tex;\n"
        "uniform float u_use_tex;\n"
        "void main() {\n"
        "    vec4 t = mix(vec4(1.0), texture(u_tex, TexCoord), u_use_tex);"
        "    FragColor = Color * t;\n"
        "}\n";

    GLuint prog = gl_create_shader_program(vs_src, fs_src);

    glUseProgram(prog);
    Mat_4 ident = mat4_identity();
    glUniformMatrix4fv(glGetUniformLocation(prog, "u_mvp"), 1, GL_FALSE, ident.m);
    glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);
    glUniform1f(glGetUniformLocation(prog, "u_use_tex"), 0.0f);
    glUseProgram(0);

    return prog;
}

static struct Texture gl_load_texture(const char *path, GLint sampling_type)
{
    struct Texture tex;
    unsigned char *tex_data = stbi_load(path, &tex.w, &tex.h, &tex.ch, 0);

    if (tex.ch == 4) { tex.internal_format = GL_RGBA8; tex.format = GL_RGBA; }
    else if (tex.ch == 3) { tex.internal_format = GL_RGB8; tex.format = GL_RGB; }
    else if (tex.ch == 2) { tex.internal_format = GL_RG8; tex.format = GL_RG; }
    else if (tex.ch == 1) { tex.internal_format = GL_R8; tex.format = GL_RED; }

    glGenTextures(1, &tex.texture_id);
    glBindTexture(GL_TEXTURE_2D, tex.texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, tex.internal_format, tex.w, tex.h, 0, tex.format, GL_UNSIGNED_BYTE, tex_data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sampling_type);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sampling_type);

    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(tex_data);

    return tex;
}

static void gl_delete_texture(struct Texture *tex)
{
    glDeleteTextures(1, &tex->texture_id);
}

static inline size_t vb_vert_size(const struct Vert_Buf *vb)
{
    return sizeof(vb->verts[0]) * vb->vert_count;
}

static inline size_t vb_max_vert_size(const struct Vert_Buf *vb)
{
    return sizeof(vb->verts[0]) * VERT_MAX;
}

static inline size_t vb_index_size(const struct Vert_Buf *vb)
{
    return sizeof(vb->indices[0]) * vb->index_count;
}

static inline size_t vb_max_index_size(const struct Vert_Buf *vb)
{
    return sizeof(vb->indices[0]) * INDEX_MAX;
}

static inline struct Vert_Buf *vb_make()
{
    struct Vert_Buf *vb = malloc(sizeof(struct Vert_Buf));
    vb->vert_count = 0;
    vb->index_count = 0;

    glGenVertexArrays(1, &vb->vao);
    glGenBuffers(1, &vb->vbo);
    glGenBuffers(1, &vb->ebo);

    glBindVertexArray(vb->vao);
    glBindBuffer(GL_ARRAY_BUFFER, vb->vbo);
    glBufferData(GL_ARRAY_BUFFER, vb_max_vert_size(vb), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vb->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vb_max_index_size(vb), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vert), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vert), (void *)(offsetof(struct Vert, u)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vert), (void *)(offsetof(struct Vert, color)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    return vb;
}

static inline void vb_clear(struct Vert_Buf *vb)
{
    vb->vert_count = 0;
    vb->index_count = 0;
}

static inline void vb_free(struct Vert_Buf *vb)
{
    glDeleteBuffers(1, &vb->vbo);
    glDeleteBuffers(1, &vb->ebo);
    glDeleteVertexArrays(1, &vb->vao);
    free(vb);
}

static inline void vb_add_vert(struct Vert_Buf *vert_buffer, struct Vert vert)
{
    if (vert_buffer->vert_count < VERT_MAX)
    {
        vert_buffer->verts[vert_buffer->vert_count++] = vert;
    }
}

static inline int vb_next_vert_index(const struct Vert_Buf *vb)
{
    return vb->vert_count;
}

static inline void vb_add_indices(struct Vert_Buf *vb, int base, int *indices, int index_count)
{
    for (int i = 0; i < index_count; i++)
    {
        if (vb->index_count < INDEX_MAX)
        {
            vb->indices[vb->index_count++] = base + indices[i];
        }
    }
}

static inline void vb_draw_call(const struct Vert_Buf *vb)
{
    glBindVertexArray(vb->vao);
    glBindBuffer(GL_ARRAY_BUFFER, vb->vbo);
    glBufferData(GL_ARRAY_BUFFER, vb_max_vert_size(vb), NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vb_vert_size(vb), vb->verts);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vb->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, vb_max_index_size(vb), NULL, GL_DYNAMIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vb_index_size(vb), vb->indices);

    glDrawElements(GL_TRIANGLES, vb->index_count, GL_UNSIGNED_INT, 0);
}
