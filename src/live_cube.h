#pragma once

#include "common.h" // IWYU pragma: keep

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "platform_types.h"

typedef struct {
    Vec_2 window_dim;
    Vec_2 framebuffer_dim;
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

void on_init(Live_Cube_State *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene);
void on_reload(Live_Cube_State *state);
void on_render(Live_Cube_State *state, const Platform_Timing *t);
void on_platform_event(Live_Cube_State *state, const Platform_Event *e);
void on_destroy(Live_Cube_State *state);
