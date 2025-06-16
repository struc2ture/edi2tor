#include <dlfcn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <SDL3/sdl.h>
#include <OpenGL/gl3.h>

#define PROGRAM_NAME "edi2tor"
#define DL_PATH "/Users/struc/dev/jects/edi2tor/bin/editor.dylib"
#define INITIAL_WINDOW_WIDTH 1000
#define INITIAL_WINDOW_HEIGHT 900

typedef void (*on_init_t)(SDL_Window *window, void *state);
typedef void (*on_reload_t)(void *state);
typedef void (*on_render_t)(void *state, bool *running);
typedef void (*on_resize_t)(void *state, int px_w, int px_h, int win_w, int win_h);
typedef void (*on_input_event_t)(void *state, const SDL_Event *e);
typedef void (*on_destroy_t)(void *state);

typedef struct {
    void *dl_handle;
    char *dl_path;
    time_t dl_timestamp;
    on_init_t on_init;
    on_reload_t on_reload;
    on_render_t on_render;
    on_resize_t on_resize;
    on_input_event_t on_input_event;
    on_destroy_t on_destroy;
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
    dl_info.on_init = xdlsym(dl_info.dl_handle, "on_init");
    dl_info.on_reload = xdlsym(dl_info.dl_handle, "on_reload");
    dl_info.on_render = xdlsym(dl_info.dl_handle, "on_render");
    dl_info.on_resize = xdlsym(dl_info.dl_handle, "on_resize");
    dl_info.on_input_event = xdlsym(dl_info.dl_handle, "on_input_event");
    dl_info.on_destroy = xdlsym(dl_info.dl_handle, "on_destroy");
    return dl_info;
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

    SDL_Window *window = SDL_CreateWindow(PROGRAM_NAME, INITIAL_WINDOW_WIDTH, INITIAL_WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);

    printf("[PLATFORM] OpenGL Vendor: %s\n", glGetString(GL_VENDOR));
    printf("[PLATFORM] OpenGL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("[PLATFORM] OpenGL Version: %s\n", glGetString(GL_VERSION));
    printf("[PLATFORM] OpenGL GLSL Version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    printf("[PLATFORM] OpenGL Extensions: %s\n", glGetString(GL_EXTENSIONS));

    Dl_Info dl_info = load_dl(DL_PATH);
    void *dl_state = calloc(1, 4096);
    dl_info.on_init(window, dl_state);
    dl_info.on_reload(window);

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
                case SDL_EVENT_QUIT:
                {
                    running = false;
                } break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED:
                {
                    int px_w, px_h, win_w, win_h;
                    SDL_GetWindowSizeInPixels(window, &px_w, &px_h);
                    SDL_GetWindowSize(window, &win_w, &win_h);
                    dl_info.on_resize(dl_state, px_w, px_h, win_w, win_h);
                } break;

                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                case SDL_EVENT_TEXT_INPUT:
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                case SDL_EVENT_MOUSE_WHEEL:
                {
                    dl_info.on_input_event(dl_state, &e);
                } break;
            }
        }
        time_t dl_current_timestamp = get_file_timestamp(dl_info.dl_path);
        if (dl_current_timestamp != dl_info.dl_timestamp)
        {
            dlclose(dl_info.dl_handle);
            dl_info = load_dl(DL_PATH);
            dl_info.on_reload(window);
            printf("[PLATFORM] Reloaded dl\n");
        }

        // TODO: Instead of passing bool pointer, have a platform function to stop running?
        dl_info.on_render(dl_state, &running);

        SDL_GL_SwapWindow(window);
    }

    dl_info.on_destroy(dl_state);
    free(dl_state);
    dlclose(dl_info.dl_handle);

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
