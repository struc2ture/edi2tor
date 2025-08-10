#include <stdint.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "../hub/hub.h"
#include "../lib/common.h"
#include "../lib/gl_glue.h"

struct Triangle_State
{
    GLuint prog;
    struct Vert_Buf *vb;
    float w, h;
    float angle;
    bool mouse_pressed;
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

    vb_clear(s->vb);

    int index_base = vb_next_vert_index(s->vb);

    Mat_4 rot = mat4_rotate_axis(0.0f, 0.0f, -1.0f, s->angle);

    Vec_3 a = (Vec_3){-0.5f, -0.5f, 0.0f};
    Vec_3 b = (Vec_3){ 0.0f,  0.5f, 0.0f};
    Vec_3 c = (Vec_3){ 0.5f, -0.5f, 0.0f};

    a = mat4_mul_vec3(rot, a);
    b = mat4_mul_vec3(rot, b);
    c = mat4_mul_vec3(rot, c);

    vb_add_vert(s->vb, (struct Vert){a.x, a.y, a.z, 0.0f, 0.0f, (struct Col4f){1.0f, 0.0f, 0.0f, 1.0f}});
    vb_add_vert(s->vb, (struct Vert){b.x, b.y, b.z, 0.0f, 0.0f, (struct Col4f){0.0f, 1.0f, 0.0f, 1.0f}});
    vb_add_vert(s->vb, (struct Vert){c.x, c.y, c.z, 0.0f, 0.0f, (struct Col4f){0.0f, 0.0f, 1.0f, 1.0f}});

    vb_add_indices(s->vb, index_base, (int[]){0, 1, 2}, 3);

    vb_draw_call(s->vb);
}

void on_platform_event(struct Triangle_State *s, const struct Hub_Event *e)
{
    switch (e->kind)
    {
        case HUB_EVENT_MOUSE_BUTTON:
        {
            if (e->mouse_button.button == GLFW_MOUSE_BUTTON_LEFT)
            {
                if (e->mouse_button.action == GLFW_PRESS)
                {
                    s->mouse_pressed = true;
                }
                else if (e->mouse_button.action == GLFW_RELEASE)
                {
                    s->mouse_pressed = false;
                }
            }
        } break;

        case HUB_EVENT_MOUSE_MOTION:
        {
            if (s->mouse_pressed)
            {
                s->angle += 0.01f * e->mouse_motion.delta.x;
            }
        } break;

        default: break;
    }
}

void on_destroy(struct Triangle_State *s)
{
    if (s->prog) glDeleteProgram(s->prog);
    if (s->vb) vb_free(s->vb);
}
