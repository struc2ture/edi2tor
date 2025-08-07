#include <stdbool.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3
#include <cimgui.h>
#include <cimgui_impl.h>
#define igGetIO igGetIO_Nil

#include "../hub/hub.h"
#include "../hub/scene.h"

struct State
{
    const struct Hub_Context *hub_context;
    struct ImGuiContext* imgui_context;
    struct ImGuiIO* imgui_io;
    GLFWwindow *window;
    bool is_demo_open;
    bool is_scene_debug_open;
};

void on_init(struct State *state, const struct Hub_Context *hub_context)
{
    state->hub_context = hub_context;
    state->imgui_context = igCreateContext(NULL);
    state->imgui_io = igGetIO();
    state->window = hub_context->window;

    const char* glsl_version = "#version 330 core";
    ImGui_ImplGlfw_InitForOpenGL(state->window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    igStyleColorsDark(NULL);

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    ImGuiStyle* style = igGetStyle();
    ImGuiStyle_ScaleAllSizes(style, main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style->FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
}

void on_reload(void *state)
{
}

void on_frame(struct State *state, const struct Hub_Timing *t)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    igNewFrame();

    igShowDemoWindow(&state->is_demo_open);

    if (igBegin("Hello World", &state->is_scene_debug_open, ImGuiWindowFlags_None))
    {
        for (int i = 0; i < *state->hub_context->scene_count; i++)
        {
            const struct Scene *s = &state->hub_context->scenes[i];
            igBulletText(s->original_path);
        }
    }
    igEnd();

    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}

void on_platform_event(void *state, const struct Hub_Event *e)
{
}

void on_destroy(void *state)
{
}
