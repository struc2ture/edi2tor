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
} Live_Scene_State;

void on_init(Live_Scene_State *state, float w, float h);
void on_reload(Live_Scene_State *state);
void on_render(Live_Scene_State *state, float delta_time);
void on_destroy(Live_Scene_State *state);
