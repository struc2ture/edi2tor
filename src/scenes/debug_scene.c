#include <stdbool.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_USE_GLFW
#define CIMGUI_USE_OPENGL3
#include <cimgui.h>
#include <cimgui_impl.h>
#define igGetIO igGetIO_Nil

#include "../hub/hub.h"
#include "../hub/scene.h"
#include "../editor/editor.h"
#include "../lib/util.h"
#include "cube/cube.h"

struct State
{
    struct Hub_Context *hub_context;
    struct ImGuiContext* imgui_context;
    struct ImGuiIO* imgui_io;
    GLFWwindow *window;
    bool is_demo_open;
    bool is_hub_debug_open;
    bool is_editor_debug_open;
    bool is_cube_debug_open;
    char open_scene_buf[256];
};

void on_init(struct State *state, struct Hub_Context *hub_context)
{
    state->hub_context = hub_context;
    state->imgui_context = igCreateContext(NULL);
    state->imgui_io = igGetIO();
    state->window = hub_context->window;

    const char* glsl_version = "#version 330 core";
    ImGui_ImplGlfw_InitForOpenGL(state->window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    igStyleColorsLight(NULL);

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    ImGuiStyle* style = igGetStyle();
    ImGuiStyle_ScaleAllSizes(style, main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style->FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
}

void on_reload(void *state)
{
}

void hub_debug(struct State *state)
{
    if (igBegin("Hub Debug", &state->is_hub_debug_open, ImGuiWindowFlags_None))
    {
        if (igCollapsingHeader_TreeNodeFlags("Scenes", 0))
        {
            igInputText("##OpenScene", state->open_scene_buf, array_size(state->open_scene_buf), 0, NULL, NULL);
            igSameLine(0, 10);
            if (igButton("Open Scene", (ImVec2){0}))
            {
                state->hub_context->open_scene(state->open_scene_buf);
            }

            igSpacing();

            scene_map_for_each(&state->hub_context->scene_map, s)
            {
                igPushID_Ptr(s);

                igBulletText(scene_map_get_name(&state->hub_context->scene_map, s));

                igSameLine(0, 10);

                bool temp;
                igCheckbox("I", &temp);

                igSameLine(0, 10);

                if (igButton("X", (ImVec2){0}))
                {
                    state->hub_context->close_scene(s);
                }

                igSameLine(0, 10);

                if (igButton("^", (ImVec2){0}))
                {
                    scene_map_swap_up(&state->hub_context->scene_map, s);
                }

                igSameLine(0, 10);

                if (igButton("v", (ImVec2){0}))
                {
                    scene_map_swap_down(&state->hub_context->scene_map, s);
                }

                igPopID();
            }
        }
    }
    igEnd();
}

void editor_debug(struct State *state)
{
    const struct Scene *editor_scene = scene_map_get_by_name(&state->hub_context->scene_map, "bin/editor.dylib");
    if (editor_scene)
    {
        if (igBegin("Editor Debug", &state->is_editor_debug_open, ImGuiWindowFlags_None))
        {
            const Editor_State *editor_state = (Editor_State *)editor_scene->state;
            for (int i = 0; i < editor_state->view_count; i++)
            {
                const View *v = editor_state->views[i];
                if (v->kind == VIEW_KIND_BUFFER)
                {
                    if (v->bv.buffer->file_path) igBulletText(v->bv.buffer->file_path);
                    else igBulletText("Buffer View");
                }
                else igBulletText("Image View");
            }
        }
        igEnd();
    }
}

void cube_debug(struct State *state)
{
    const struct Scene *cube_scene = scene_map_get_by_name(&state->hub_context->scene_map, "bin/cube.dylib");
    if (cube_scene)
    {
        if (igBegin("Cube Debug", &state->is_cube_debug_open, ImGuiWindowFlags_None))
        {
            Cube_State *cube_state = (Cube_State *)cube_scene->state;
            igSliderAngle("Roll", &cube_state->angle_roll, -360, 360, "%.0f deg", 0);
            igSliderAngle("Yaw", &cube_state->angle_yaw, -360, 360, "%.0f deg", 0);
        }
        igEnd();
    }
}

void on_frame(struct State *state, const struct Hub_Timing *t)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    igNewFrame();

    igShowDemoWindow(&state->is_demo_open);

    hub_debug(state);
    editor_debug(state);
    cube_debug(state);

    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}

void on_platform_event(void *state, const struct Hub_Event *e)
{
}

void on_destroy(void *state)
{
}

#include "../hub/scene.c"
