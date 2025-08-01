#pragma once

#include <stdbool.h>
#include <time.h>

#include <GLFW/glfw3.h>

#include "hub.h"

typedef void (*scene_on_init_t)(void *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene, GLuint fbo, int argc, char **argv);
typedef void (*scene_on_reload_t)(void *state);
typedef void (*scene_on_render_t)(void *state, const struct Hub_Timing *t);
typedef void (*scene_on_platform_event_t)(void *state, const struct Hub_Event *e);
typedef void (*scene_on_destroy_t)(void *state);

struct Scene
{
    void *handle;
    char *original_path;
    char *copied_path;
    time_t timestamp;

    void *state;

    scene_on_init_t on_init;
    scene_on_reload_t on_reload;
    scene_on_render_t on_frame;
    scene_on_platform_event_t on_platform_event;
    scene_on_destroy_t on_destroy;
};

struct Scene scene_open(const char *path);
void scene_close(struct Scene *s);
bool scene_hotreload(struct Scene *s);
