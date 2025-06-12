#pragma once

#include <OpenGL/gl3.h>

typedef struct {
    GLuint prog;
    GLuint vao;
} User_State;

void user_init(User_State *state);
void user_draw(User_State *state);
