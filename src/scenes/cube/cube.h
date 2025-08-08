#pragma once

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "../../lib/common.h"

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
    Dir move_dir;
    Vec_2 position;
    Vec_2 velocity;
    bool is_captured;
    GLFWwindow *window;
} Cube_State;
