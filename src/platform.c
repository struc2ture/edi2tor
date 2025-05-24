#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#define INITIAL_WINDOW_WIDTH 800
#define INITIAL_WINDOW_HEIGHT 600

typedef void (*_init_t)(GLFWwindow *window, void *_state);
typedef void (*_hotreload_init_t)(GLFWwindow *window);
typedef void (*_render_t)(GLFWwindow *window, void *_state);

typedef struct {
    void *dl_handle;
    char *dl_path;
    time_t dl_timestamp;
    _init_t _init;
    _hotreload_init_t _hotreload_init;
    _render_t _render;
} Dl_Info;

time_t get_file_timestamp(const char *path) {
    struct stat attr;
    if (stat(path, &attr) == 0) {
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
    dl_info.dl_handle = xdlopen(path);
    dl_info.dl_path = strdup(path);
    dl_info.dl_timestamp = get_file_timestamp(path);
    dl_info._init = xdlsym(dl_info.dl_handle, "_init");
    dl_info._hotreload_init = xdlsym(dl_info.dl_handle, "_hotreload_init");
    dl_info._render = xdlsym(dl_info.dl_handle, "_render");
    return dl_info;
}

int main()
{
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow *window = glfwCreateWindow(INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, "EDITOR", NULL, NULL);
    if (!window) return -1;
    glfwMakeContextCurrent(window);
    printf("[PLATFORM] OpenGL version: %s\n", glGetString(GL_VERSION));

    Dl_Info dl_info = load_dl("./bin/editor.dylib");
    void *_state = calloc(1, 4096);
    dl_info._init(window, _state);
    dl_info._hotreload_init(window);

    while (!glfwWindowShouldClose(window)) {
        time_t dl_current_timestamp = get_file_timestamp(dl_info.dl_path);
        if (dl_current_timestamp != dl_info.dl_timestamp) {
            dlclose(dl_info.dl_handle);
            dl_info = load_dl(dl_info.dl_path);
            dl_info._hotreload_init(window);
            printf("[PLATFORM] Reloaded dl\n");
        }
        dl_info._render(window, _state);
    }

    dlclose(dl_info.dl_handle);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
