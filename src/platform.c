#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "platform_event.h"

#define PROGRAM_NAME "edi2tor"
#define DL_PATH "/Users/struc/dev/jects/edi2tor/bin/editor.dylib"
#define INITIAL_WINDOW_WIDTH 1000
#define INITIAL_WINDOW_HEIGHT 900

typedef void (*on_init_t)(GLFWwindow *window, void *state);
typedef void (*on_reload_t)(void *state);
typedef void (*on_render_t)(void *state);
typedef void (*on_platform_event_t)(void *state, const Platform_Event *event);
typedef void (*on_destroy_t)(void *state);

typedef struct {
    void *handle;
    char *dl_path;
    time_t dl_timestamp;
    on_init_t on_init;
    on_reload_t on_reload;
    on_render_t on_render;
    on_platform_event_t on_platform_event;
    on_destroy_t on_destroy;
} Dl_Info;

static Dl_Info g_dl;
static void *g_dl_state;

time_t get_file_timestamp(const char *path)
{
    struct stat attr;
    if (stat(path, &attr) == 0)
    {
        return attr.st_mtime;
    }
    fprintf(stderr, "[PLATFORM] Failed to get timestamp for file at %s\n", path);
    exit(1);
}

void *xdlopen(const char *dl_path)
{
    void *handle = dlopen(dl_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "[PLATFORM] dlopen error: %s\n", dlerror());
        exit(1);
    }
    return handle;
}

void * xdlsym(void *handle, const char *name)
{
    void *sym = dlsym(handle, name);
    if (!sym) {
        fprintf(stderr, "[PLATFORM] dlsym error: %s\n", dlerror());
        exit(1);
    }
    return sym;
}

Dl_Info load_dl(const char *path)
{
    Dl_Info dl_info = {0};
    dl_info.handle = xdlopen(path);
    dl_info.dl_path = strdup(path);
    dl_info.dl_timestamp = get_file_timestamp(path);
    dl_info.on_init = xdlsym(dl_info.handle, "on_init");
    dl_info.on_reload = xdlsym(dl_info.handle, "on_reload");
    dl_info.on_render = xdlsym(dl_info.handle, "on_render");
    dl_info.on_platform_event = xdlsym(dl_info.handle, "on_platform_event");
    dl_info.on_destroy = xdlsym(dl_info.handle, "on_destroy");
    return dl_info;
}

// --------------------------------------------------------

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void) window;

    Platform_Event e;
    e.kind = PLATFORM_EVENT_CHAR;
    e.character.codepoint = codepoint;

    g_dl.on_platform_event(g_dl_state, &e);
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

    g_dl.on_platform_event(g_dl_state, &e);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void)window;

    Platform_Event e;
    e.kind = PLATFORM_EVENT_MOUSE_BUTTON;
    e.mouse_button.button = button;
    e.mouse_button.action = action;
    e.mouse_button.mods = mods;

    g_dl.on_platform_event(g_dl_state, &e);
}

void scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window;

    Platform_Event e;
    e.kind = PLATFORM_EVENT_SCROLL;
    e.scroll.x_offset = x_offset;
    e.scroll.y_offset = y_offset;

    g_dl.on_platform_event(g_dl_state, &e);
}

void framebuffer_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window;

    Platform_Event e;
    e.kind = PLATFORM_EVENT_WINDOW_RESIZE;
    e.window_resize.px_w = w;
    e.window_resize.px_h = h;
    glfwGetWindowSize(window, &e.window_resize.logical_w, &e.window_resize.logical_h);

    g_dl.on_platform_event(g_dl_state, &e);
}

void window_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window;
    Platform_Event e;
    e.kind = PLATFORM_EVENT_WINDOW_RESIZE;
    e.window_resize.logical_w = w;
    e.window_resize.logical_h = h;
    glfwGetFramebufferSize(window, &e.window_resize.px_w, &e.window_resize.px_h);

    g_dl.on_platform_event(g_dl_state, &e);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow *window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, PROGRAM_NAME, NULL, NULL);
    glfwMakeContextCurrent(window);

    printf("[PLATFORM] OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    printf("[PLATFORM] OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("[PLATFORM] OpenGL Version: %s\n", glGetString(GL_VERSION));

    g_dl = load_dl(DL_PATH);
    g_dl_state = calloc(1, 4096);

    g_dl.on_init(window, g_dl_state);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    // glfwSetWindowRefreshCallback(window, refresh_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    while (!glfwWindowShouldClose(window))
    {
        time_t dl_current_timestamp = get_file_timestamp(g_dl.dl_path);
        if (dl_current_timestamp != g_dl.dl_timestamp)
        {
            dlclose(g_dl.handle);
            g_dl = load_dl(DL_PATH);
            g_dl.on_reload(g_dl_state);
            printf("[PLATFORM] Reloaded dl\n");
        }

        g_dl.on_render(g_dl_state);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    g_dl.on_destroy(g_dl_state);
    free(g_dl_state);
    dlclose(g_dl.handle);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
