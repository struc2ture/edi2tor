#include <stdio.h>
#include <stdlib.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "hub.h"
#include "hub_internal.h"
#include "scene.h"
#include "../lib/common.h"
#include "../lib/glfw_helpers.h"

#define INITIAL_PROGRAM_NAME "hub"
#define INITIAL_WINDOW_WIDTH 1000
#define INITIAL_WINDOW_HEIGHT 900
#define FPS_MEASUREMENT_FREQ 0.1f

static struct Hub_Context hub_context;
static struct Hub_Timing hub_timing;
static Vec_2 mouse_prev_pos;

void perform_timing_calculations(struct Hub_Timing *t)
{
    t->prev_frame_time = (float)glfwGetTime();
    t->prev_delta_time = t->prev_frame_time - t->prev_prev_frame_time;
    t->prev_prev_frame_time = t->prev_frame_time;
    t->fps_instant = 1.0f / t->prev_delta_time;

    if (t->prev_frame_time - t->last_fps_measurement_time > FPS_MEASUREMENT_FREQ)
    {
        t->fps_avg = (float)t->frame_running_count / (t->prev_frame_time - t->last_fps_measurement_time);
        t->last_fps_measurement_time = t->prev_frame_time;
        t->frame_running_count = 0;
    }

    t->frame_total_count++;
    t->frame_running_count++;
}

void dispatch_hub_event(const struct Hub_Event *e)
{
    scene_map_for_each(&hub_context.scene_map, s)
    {
        s->on_platform_event(s->state, e);
    }
    struct Scene *debug_scene = &hub_context.scene_map.debug_scene;
    debug_scene->on_platform_event(debug_scene->state, e);
}

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void) window;
    struct Hub_Event e;
    e.kind = HUB_EVENT_CHAR;
    e.character.codepoint = codepoint;
    dispatch_hub_event(&e);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void) window;
    struct Hub_Event e;
    e.kind = HUB_EVENT_KEY;
    e.key.key = key;
    e.key.scancode = scancode;
    e.key.action = action;
    e.key.mods = mods;
    dispatch_hub_event(&e);
}

void mouse_cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    (void)window;
    struct Hub_Event e;
    e.kind = HUB_EVENT_MOUSE_MOTION;
    e.mouse_motion.pos = (Vec_2){(float)xpos, (float)ypos};
    e.mouse_motion.delta = vec2_sub(e.mouse_motion.pos, mouse_prev_pos);
    mouse_prev_pos = e.mouse_motion.pos;
    dispatch_hub_event(&e);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    struct Hub_Event e;
    e.kind = HUB_EVENT_MOUSE_BUTTON;
    e.mouse_button.button = button;
    e.mouse_button.action = action;
    e.mouse_button.mods = mods;
    e.mouse_button.pos = glfwh_get_mouse_position(window);
    dispatch_hub_event(&e);
}

void scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window;
    struct Hub_Event e;
    e.kind = HUB_EVENT_MOUSE_SCROLL;
    e.mouse_scroll.scroll = (Vec_2){(float)x_offset, (float)y_offset};
    e.mouse_scroll.pos = glfwh_get_mouse_position(window);
    dispatch_hub_event(&e);
}

void framebuffer_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window;
    struct Hub_Event e;
    e.kind = HUB_EVENT_WINDOW_RESIZE;
    e.window_resize.px_w = w;
    e.window_resize.px_h = h;
    glfwGetWindowSize(window, &e.window_resize.logical_w, &e.window_resize.logical_h);
    dispatch_hub_event(&e);
}

void window_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window;
    struct Hub_Event e;
    e.kind = HUB_EVENT_WINDOW_RESIZE;
    e.window_resize.logical_w = w;
    e.window_resize.logical_h = h;
    glfwGetFramebufferSize(window, &e.window_resize.px_w, &e.window_resize.px_h);
    dispatch_hub_event(&e);
}

void open_scene(const char *path)
{
    struct Scene *s = scene_map_add(&hub_context.scene_map, scene_open(path), path);
    // TODO: Handle failed allocation
    s->state = calloc(1, 4096);
    s->on_init(s->state, &hub_context);
    s->on_reload(s->state);
}

void open_debug_scene(const char *path)
{
    struct Scene *s = scene_map_add_debug_scene(&hub_context.scene_map, scene_open(path));
    // TODO: Handle failed allocation
    s->state = calloc(1, 4096);
    s->on_init(s->state, &hub_context);
    s->on_reload(s->state);
}

void close_scene(struct Scene *s)
{
    s->on_destroy(s->state);
    free(s->state);
    scene_map_remove(&hub_context.scene_map, s);
    scene_close(s);
}

void run_scratch(const char *path)
{
    hub_trace("run scratch %s", path);
}

int main(int argc, char **argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow *window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, INITIAL_PROGRAM_NAME, NULL, NULL);
    glfwMakeContextCurrent(window);

    hub_trace("OpenGL Vendor: %s", glGetString(GL_VENDOR));
    hub_trace("OpenGL Renderer: %s", glGetString(GL_RENDERER));
    hub_trace("OpenGL Version: %s", glGetString(GL_VERSION));

    hub_trace("This is a message to make the codebase dirtier");

    int window_w, window_h, window_px_w, window_px_h;
    glfwGetWindowSize(window, &window_w, &window_h);
    glfwGetFramebufferSize(window, &window_px_w, &window_px_h);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    // glfwSetWindowRefreshCallback(window, refresh_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_pos_callback);

    glfwSwapInterval(1);

    hub_context.window = window;
    hub_context.window_w = window_w;
    hub_context.window_h = window_h;
    hub_context.window_px_w = window_px_w;
    hub_context.window_px_h = window_px_h;
    hub_context.is_live_scene = false;
    hub_context.fbo = 0;
    hub_context.argc = argc;
    hub_context.argv = argv;

    hub_context.open_scene = open_scene;
    hub_context.close_scene = close_scene;
    hub_context.run_scratch = run_scratch;

    open_debug_scene("bin/debug.dylib");
    struct Scene *debug_scene = scene_map_get_debug_scene(&hub_context.scene_map);

    for (int i = 1; i < argc; i++)
    {
        open_scene(argv[i]);
    }

    dispatch_hub_event(&(struct Hub_Event) {
        .kind = HUB_EVENT_INPUT_CAPTURED,
        .input_captured.captured = true
    });

    while (!glfwWindowShouldClose(window))
    {
        // TODO: Re-implement F12 to break.
        // __builtin_debugtrap();

        glViewport(0, 0, hub_context.window_px_w, hub_context.window_px_h);
        glClearColor(0.9f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        scene_map_for_each(&hub_context.scene_map, s)
        {
            if (scene_hotreload(s))
            {
                s->on_reload(s->state);
            }
        }
        if (scene_hotreload(debug_scene))
        {
            debug_scene->on_reload(debug_scene->state);
        }

        perform_timing_calculations(&hub_timing);

        scene_map_for_each(&hub_context.scene_map, s)
        {
            s->on_frame(s->state, &hub_timing);
        }
        debug_scene->on_frame(debug_scene->state, &hub_timing);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (int i = 0; i < hub_context.scene_map.size; i++)
    {
        close_scene(&hub_context.scene_map.scenes[0]);
    }
    close_scene(debug_scene);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#include "scene.c"
