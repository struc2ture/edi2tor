#pragma once

#include "common.h" // IWYU pragma: keep

#include <OpenGL/gl3.h>

#include "platform_types.h"

typedef struct {
    GLuint prog;
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    float angle_roll;
    float angle_yaw;
    float w;
    float h;
    bool is_mouse_drag;
    Cardinal_Direction move_dir;
    Vec_2 position;
    Vec_2 velocity;
} Live_Cube_State;

void on_init(Live_Cube_State *state, float w, float h);
void on_reload(Live_Cube_State *state);
void on_render(Live_Cube_State *state, const Platform_Timing *t);
void on_platform_event(Live_Cube_State *state, const Platform_Event *e);
void on_destroy(Live_Cube_State *state);
