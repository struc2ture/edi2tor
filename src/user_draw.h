#pragma once

#include <OpenGL/gl3.h>

typedef struct {
    GLuint prog;
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    float angle;
    float w;
    float h;
} User_State;

void user_init(User_State *state, float w, float h);
void user_draw(User_State *state, float delta_time);
void user_destroy(User_State *state);
