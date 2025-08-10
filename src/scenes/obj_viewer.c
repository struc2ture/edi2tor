#include <stdint.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "../hub/hub.h"
#include "../lib/common.h"
#include "../lib/gl_glue.h"
#include "../lib/util.h"

struct Triangle_State
{
    GLuint prog;
    struct Vert_Buf *vb;
    float w, h;
};

void on_init(struct Triangle_State *s, struct Hub_Context *hub_context)
{
    s->w = hub_context->window_px_w;
    s->h = hub_context->window_px_h;
    s->prog = gl_create_basic_shader();
    s->vb = vb_make();
}

void on_reload(struct Triangle_State *s)
{
}

void on_frame(struct Triangle_State *s, const struct Hub_Timing *t)
{
    glViewport(0, 0, s->w, s->h);
    glUseProgram(s->prog);

    glEnable(GL_DEPTH_TEST);

    vb_clear(s->vb);

    int index_base = vb_next_vert_index(s->vb);

    Vec_3 verts[] =
    {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f,  0.5f},
        {-0.5f, -0.5f,  0.5f},
        {-0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f},
    };

    struct Col4f colors[] =
    {
        {0.5f, 0.0f, 0.0f, 1.0f},
        {0.0f, 0.5f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.5f, 1.0f},
        {0.5f, 0.0f, 0.5f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f},
    };

    Vec_3 axis = vec3_normalize((Vec_3){0.0f, 0.5f, 0.5f});
    Mat_4 rot = mat4_rotate_axis(axis.x, axis.y, axis.z, 1.5f);

    for (int i = 0; i < (int)array_size(verts); i++)
    {
        Vec_3 v = mat4_mul_vec3(rot, verts[i]);
        vb_add_vert(s->vb, (struct Vert){v.x, v.y, v.z, 0.0f, 0.0f, colors[i]});
    }

    int indices[] =
    {
        0, 1, 2,
        2, 3, 0,
        4, 5, 6,
        6, 7, 4,
        3, 2, 6,
        6, 7, 3,
        0, 1, 5,
        5, 4, 0,
        0, 3, 7,
        7, 4, 0,
        1, 2, 6,
        6, 5, 1
    };

    vb_add_indices(s->vb, index_base, indices, array_size(indices));

    vb_draw_call(s->vb);

    glDisable(GL_DEPTH_TEST);
}

void on_platform_event(struct Triangle_State *s, const struct Hub_Event *e)
{
}

void on_destroy(struct Triangle_State *s)
{
    if (s->prog) glDeleteProgram(s->prog);
    if (s->vb) vb_free(s->vb);
}
