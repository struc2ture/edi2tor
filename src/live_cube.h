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

void live_cube_init(User_State *state, float w, float h);
void live_cube_draw(User_State *state, float delta_time);
void live_cube_destroy(User_State *state);
