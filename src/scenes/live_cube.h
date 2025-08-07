#pragma once

#include "../common.h" // IWYU pragma: keep

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "../hub/hub.h"
// #include "platform_types.h"

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
    bool is_captured;
    GLFWwindow *window;
} Live_Cube_State;

void on_init(Live_Cube_State *state, const struct Hub_Context *hub_context);
void on_reload(Live_Cube_State *state);
void on_frame(Live_Cube_State *state, const struct Hub_Timing *t);
void on_platform_event(Live_Cube_State *state, const struct Hub_Event *e);
void on_destroy(Live_Cube_State *state);
