#pragma once

#include <stdlib.h>
#include <string.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "scene.h"
#include "../lib/common.h"

#define MAX_SCENES 8

enum Hub_Event_Kind
{
    HUB_EVENT_CHAR,
    HUB_EVENT_KEY,
    HUB_EVENT_MOUSE_BUTTON,
    HUB_EVENT_MOUSE_MOTION,
    HUB_EVENT_MOUSE_SCROLL,
    HUB_EVENT_WINDOW_RESIZE,
    HUB_EVENT_INPUT_CAPTURED,
};

struct Hub_Event
{
    union
    {
        struct
        {
            unsigned int codepoint;
        } character;

        struct
        {
            int key;
            int scancode;
            int action;
            int mods;
        } key;

        struct
        {
            int button;
            int action;
            int mods;
            Vec_2 pos;
        } mouse_button;

        struct
        {
            Vec_2 pos;
            Vec_2 delta;
        } mouse_motion;

        struct
        {
            Vec_2 scroll;
            Vec_2 pos;
        } mouse_scroll;

        struct
        {
            int px_w;
            int px_h;
            int logical_w;
            int logical_h;
        } window_resize;

        struct
        {
            bool captured;
        } input_captured;
    };
    enum Hub_Event_Kind kind;
};

struct Hub_Timing
{
    float prev_frame_time;
    float prev_prev_frame_time;
    float prev_delta_time;
    float last_fps_measurement_time;
    int frame_running_count;
    int frame_total_count;
    float fps_avg;
    float fps_instant;
};

typedef void (*hub_open_scene_t)(const char *path);
typedef void (*hub_close_scene_t)(struct Scene *s);
typedef void (*hub_run_scratch_t)(const char *path);

struct Hub_Context
{
    struct Scene_Map scene_map;

    hub_open_scene_t open_scene;
    hub_close_scene_t close_scene;
    hub_run_scratch_t run_scratch;

    GLFWwindow *window;
    float window_w;
    float window_h;
    float window_px_w;
    float window_px_h;
    bool is_live_scene;
    GLuint fbo;
    int argc;
    char **argv;
};
