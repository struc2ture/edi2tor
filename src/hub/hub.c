#include "hub.h"
#include <stdio.h>
#include <stdlib.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "../common.h"
#include "../glfw_helpers.h"
#include "hub_internal.h"
#include "scene.h"

#define INITIAL_PROGRAM_NAME "hub"
#define INITIAL_WINDOW_WIDTH 1000
#define INITIAL_WINDOW_HEIGHT 900
#define FPS_MEASUREMENT_FREQ 0.1f

static struct Hub_Timing timing;
static struct Hub_State hub_state;
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
    for (int i = 0; i < hub_state.scene_count; i++)
    {
        hub_state.scenes[i].on_platform_event(hub_state.scenes[i].state, e);
    }
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

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s path/to/library.dylib\n", argv[0]);
        return 1;
    }

    const char *scene_path = argv[1];

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

    hub_state.scenes[hub_state.scene_count] = scene_open(scene_path);
    // TODO: Handle failed allocation
    hub_state.scenes[hub_state.scene_count].state = calloc(1, 4096);
    hub_state.scene_count++;

    int window_w, window_h, window_px_w, window_px_h;
    glfwGetWindowSize(window, &window_w, &window_h);
    glfwGetFramebufferSize(window, &window_px_w, &window_px_h);

    for (int i = 0; i < hub_state.scene_count; i++)
    {
        hub_state.scenes[i].on_init(
            hub_state.scenes[i].state,
            window,
            (float)window_w,
            (float)window_h,
            (float)window_px_w,
            (float)window_px_h,
            false,
            0,
            argc,
            argv
        );
    }

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    // glfwSetWindowRefreshCallback(window, refresh_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_pos_callback);

    glfwSwapInterval(1);

    dispatch_hub_event(&(struct Hub_Event) {
        .kind = HUB_EVENT_INPUT_CAPTURED,
        .input_captured.captured = true
    });

    while (!glfwWindowShouldClose(window))
    {
        for (int i = 0; i < hub_state.scene_count; i++)
        {
            struct Scene *s = &hub_state.scenes[i];
            if (scene_hotreload(s))
            {
                s->on_reload(s->state);
            }
        }

        perform_timing_calculations(&timing);

        for (int i = 0; i < hub_state.scene_count; i++)
        {
            struct Scene *s = &hub_state.scenes[i];
            s->on_frame(s->state, &timing);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (int i = 0; i < hub_state.scene_count; i++)
    {
        struct Scene *s = &hub_state.scenes[i];
        s->on_destroy(s->state);
        free(s->state);
        scene_close(s);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#include "scene.c"
