#pragma once

#include "common.h"

#include <GLFW/glfw3.h>

static inline bool glfwh_is_shift_pressed(GLFWwindow *window)
{
    return (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
}

static inline Vec_2 glfwh_get_mouse_position(GLFWwindow *window)
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    return (Vec_2){x, y};
}
