#pragma once

#include "common.h" // IWYU pragma: keep

#include <stdbool.h>
#include <time.h>

#include <GLFW/glfw3.h>

#include "platform_types.h"

// TODO: Pack all the data into a struct
typedef void (*scene_on_init_t)(void *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene, GLuint fbo, int argc, char **argv);
typedef void (*scene_on_reload_t)(void *state);
typedef void (*scene_on_render_t)(void *state, const Platform_Timing *t);
typedef void (*scene_on_platform_event_t)(void *state, const Platform_Event *e);
typedef void (*scene_on_destroy_t)(void *state);

typedef struct Scene_Dylib {
    void *handle;
    char *original_path;
    char *copied_path;
    time_t timestamp;
    scene_on_init_t on_init;
    scene_on_reload_t on_reload;
    scene_on_render_t on_frame;
    scene_on_platform_event_t on_platform_event;
    scene_on_destroy_t on_destroy;
} Scene_Dylib;

time_t scene_loader__get_file_timestamp(const char *path);
bool scene_loader__copy_file(const char *src, const char *dest);
bool scene_loader__delete_file(const char *path);

Scene_Dylib scene_loader_dylib_open(const char *path);
void scene_loader_dylib_close(Scene_Dylib *scene_dylib);
bool scene_loader_dylib_check_and_hotreload(Scene_Dylib *scene_dylib);
