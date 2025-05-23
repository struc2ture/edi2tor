#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

void *load_dl_handle(const char *dl_path)
{
    void *handle = dlopen(dl_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen error: %s\n", dlerror());
        exit(1);
    }
    return handle;
}

typedef void (*_init_t)(GLFWwindow *window, void *_state);
_init_t load__init(void *handle)
{
    _init_t f = (_init_t)dlsym(handle, "_init");
    if (!f) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        exit(1);
    }
    return f;
}

typedef void (*_hotreload_init_t)(GLFWwindow *window);
_hotreload_init_t load__hotreload_init(void *handle)
{
    _hotreload_init_t f = (_hotreload_init_t)dlsym(handle, "_hotreload_init");
    if (!f) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        exit(1);
    }
    return f;
}

typedef void (*_render_t)(GLFWwindow *window, void *_state);
_render_t load__render(void *handle)
{
    _render_t f = (_render_t)dlsym(handle, "_render");
    if (!f) {
        fprintf(stderr, "dlsym error: %s\n", dlerror());
        exit(1);
    }
    return f;
}

int main()
{
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "EDITOR", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    printf("[PLATFORM] OpenGL version: %s\n", glGetString(GL_VERSION));

    void *dl_handle = load_dl_handle("./bin/editor.dylib");
    _init_t _init = load__init(dl_handle);
    _hotreload_init_t _hotreload_init = load__hotreload_init(dl_handle);
    _render_t _render = load__render(dl_handle);

    void *_state = calloc(1, 4096);
    _init(window, _state);
    _hotreload_init(window);

    bool is_reloading = false;
    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_F10) == GLFW_PRESS && !is_reloading) {
            dlclose(dl_handle);
            dl_handle = load_dl_handle("./bin/editor.dylib");
            _init = load__init(dl_handle);
            _render = load__render(dl_handle);
            _hotreload_init = load__hotreload_init(dl_handle);
            _hotreload_init(window);
            is_reloading = true;
            printf("[PLATFORM] Reloaded dll\n");
        } else if (glfwGetKey(window, GLFW_KEY_ENTER) != GLFW_PRESS) {
            is_reloading = false;
        }
        _render(window, _state);
    }

    dlclose(dl_handle);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
