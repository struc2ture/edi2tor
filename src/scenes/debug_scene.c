#include <stdbool.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3
#include <cimgui.h>
#include <cimgui_impl.h>
#define igGetIO igGetIO_Nil

#include "../hub/hub.h"

struct State
{
    struct ImGuiContext* imgui_context;
    struct ImGuiIO* imgui_io;
    GLFWwindow *window;
    bool is_open;
};

void on_init(struct State *state, const struct Hub_Context *hub_context)
{
    state->imgui_context = igCreateContext(NULL);
    state->imgui_io = igGetIO();
    state->window = hub_context->window;

    const char* glsl_version = "#version 330 core";
    ImGui_ImplGlfw_InitForOpenGL(state->window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Setup style
    igStyleColorsDark(NULL);
}

void on_reload(void *state)
{
}

void on_frame(struct State *state, const struct Hub_Timing *t)
{
    igShowDemoWindow(&state->is_open);
}

void on_platform_event(void *state, const struct Hub_Event *e)
{
}

void on_destroy(void *state)
{
}
