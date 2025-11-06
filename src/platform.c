#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "glfw_helpers.h"
#include "platform_types.h"
#include "scene_loader.h"

#define INITIAL_PROGRAM_NAME "plat4form"
#define INITIAL_WINDOW_WIDTH 1000
#define INITIAL_WINDOW_HEIGHT 900
#define FPS_MEASUREMENT_FREQ 0.1f

static Scene_Dylib g_scene_dylib;
static void *g_scene_state;

static v2 g_mouse_prev_pos;
static Platform_Timing g_timing;

void perform_timing_calculations(Platform_Timing *t)
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

// --------------------------------------------------------

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void) window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_CHAR;
    e.character.codepoint = codepoint;
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void) window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_KEY;
    e.key.key = key;
    e.key.scancode = scancode;
    e.key.action = action;
    e.key.mods = mods;
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

void mouse_cursor_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    (void)window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_MOUSE_MOTION;
    e.mouse_motion.pos = V2((float)xpos, (float)ypos);
    e.mouse_motion.delta = vec2_sub(e.mouse_motion.pos, g_mouse_prev_pos);
    g_mouse_prev_pos = e.mouse_motion.pos;
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    Platform_Event e;
    e.kind = PLATFORM_EVENT_MOUSE_BUTTON;
    e.mouse_button.button = button;
    e.mouse_button.action = action;
    e.mouse_button.mods = mods;
    e.mouse_button.pos = glfwh_get_mouse_position(window);
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

void scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_MOUSE_SCROLL;
    e.mouse_scroll.scroll = V2((float)x_offset, (float)y_offset);
    e.mouse_scroll.pos = glfwh_get_mouse_position(window);
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

void framebuffer_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_WINDOW_RESIZE;
    e.window_resize.px_w = w;
    e.window_resize.px_h = h;
    glfwGetWindowSize(window, &e.window_resize.logical_w, &e.window_resize.logical_h);
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

void window_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_WINDOW_RESIZE;
    e.window_resize.logical_w = w;
    e.window_resize.logical_h = h;
    glfwGetFramebufferSize(window, &e.window_resize.px_w, &e.window_resize.px_h);
    g_scene_dylib.on_platform_event(g_scene_state, &e);
}

// --------------------------------------------------------

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s path/to/library.dylib\n", argv[0]);
        return 1;
    }

    const char *dylib_path = argv[1];

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow *window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, INITIAL_PROGRAM_NAME, NULL, NULL);
    glfwMakeContextCurrent(window);

    printf("[PLATFORM] OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    printf("[PLATFORM] OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("[PLATFORM] OpenGL Version: %s\n", glGetString(GL_VERSION));

    g_scene_dylib = scene_loader_dylib_open(dylib_path);
    g_scene_state = calloc(1, 4096);

    int window_w, window_h, window_px_w, window_px_h;
    glfwGetWindowSize(window, &window_w, &window_h);
    glfwGetFramebufferSize(window, &window_px_w, &window_px_h);

    g_scene_dylib.on_init(
        g_scene_state,
        window,
        (float)window_w,
        (float)window_h,
        (float)window_px_w,
        (float)window_px_h,
        false,
        0,
        argc,
        argv);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    // glfwSetWindowRefreshCallback(window, refresh_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_cursor_pos_callback);

    glfwSwapInterval(1);

    // Scene run by platform directly will always have captured input
    g_scene_dylib.on_platform_event(g_scene_state, &(Platform_Event){
        .kind = PLATFORM_EVENT_INPUT_CAPTURED,
        .input_captured.captured = true
    });

    while (!glfwWindowShouldClose(window))
    {
        if (scene_loader_dylib_check_and_hotreload(&g_scene_dylib))
        {
            g_scene_dylib.on_reload(g_scene_state);
        }

        perform_timing_calculations(&g_timing);

        g_scene_dylib.on_frame(g_scene_state, &g_timing);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_scene_dylib.on_destroy(g_scene_state);

    free(g_scene_state);
    scene_loader_dylib_close(&g_scene_dylib);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

#include "scene_loader.c"
