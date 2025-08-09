#include <stdint.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "../hub/hub.h"
#include "../lib/gl_glue.h"

struct Triangle_State
{
    GLuint prog;
    struct Vert_Buf *vb;
    float w, h;
};

void on_init(struct Triangle_State *s, struct Hub_Context *hub_context)
{
    s->w = hub_context->window_w;
    s->h = hub_context->window_h;
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

    vb_clear(s->vb);

    int index_base = vb_next_vert_index(s->vb);

    vb_add_vert(s->vb, (struct Vert){-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, (struct Col4f){1.0f, 0.0f, 0.0f, 1.0f}});
    vb_add_vert(s->vb, (struct Vert){ 0.0f,  0.5f, 0.0f, 0.0f, 0.0f, (struct Col4f){0.0f, 1.0f, 0.0f, 1.0f}});
    vb_add_vert(s->vb, (struct Vert){ 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, (struct Col4f){0.0f, 0.0f, 1.0f, 1.0f}});

    vb_add_indices(s->vb, index_base, (int[]){0, 1, 2}, 3);

    vb_draw_call(s->vb);
}

void on_platform_event(struct Triangle_State *s, const struct Hub_Event *e)
{
}

void on_destroy(struct Triangle_State *s)
{
    if (s->prog) glDeleteProgram(s->prog);
    if (s->vb) vb_free(s->vb);
}
