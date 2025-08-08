#pragma once

#include <stdbool.h>
#include <time.h>

#include <GLFW/glfw3.h>

struct Hub_Context;
struct Hub_Timing;
struct Hub_Event;

typedef void (*scene_on_init_t)(void *state, struct Hub_Context *hub_context);
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

struct Scene_Map
{
    struct Scene debug_scene;
    struct Scene *scenes;
    char **names;
    int size;
    int cap;
};

struct Scene scene_open(const char *path);
void scene_close(struct Scene *s);
bool scene_hotreload(struct Scene *s);

struct Scene *scene_map_add(struct Scene_Map *m, struct Scene scene, const char *name);
struct Scene *scene_map_add_debug_scene(struct Scene_Map *m, struct Scene scene);
struct Scene *scene_map_get_debug_scene(struct Scene_Map *m);
struct Scene *scene_map_get_by_name(struct Scene_Map *m, const char *name);
char *scene_map_get_name(struct Scene_Map *m, struct Scene *s);
void scene_map_remove(struct Scene_Map *m, struct Scene *s);
bool scene_map_swap_up(struct Scene_Map *m, struct Scene *s);
bool scene_map_swap_down(struct Scene_Map *m, struct Scene *s);

static inline struct Scene *scene_map_begin(struct Scene_Map *m)
{
    return m->scenes;
}

static inline struct Scene *scene_map_end(struct Scene_Map *m)
{
    return m->scenes + m->size;
}

#define scene_map_for_each(M, IT) \
    for (struct Scene *IT = scene_map_begin(M); IT != scene_map_end(M); ++IT)
