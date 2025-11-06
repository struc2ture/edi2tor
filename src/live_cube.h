#pragma once

#include "common.h" // IWYU pragma: keep

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "platform_types.h"

typedef struct Live_Cube_State {
    v2 window_dim;
    v2 framebuffer_dim;
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
    v2 position;
    v2 velocity;
    bool is_captured;
    GLFWwindow *window;
} Live_Cube_State;

void on_init(Live_Cube_State *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene, GLuint fbo, int argc, char **argv);
void on_reload(Live_Cube_State *state);
void on_frame(Live_Cube_State *state, const Platform_Timing *t);
void on_platform_event(Live_Cube_State *state, const Platform_Event *e);
void on_destroy(Live_Cube_State *state);
