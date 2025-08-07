#include "../hub/hub.h"
#include "../editor/editor.h"

void on_init(Editor_State *state, struct Hub_Context *hub_context)
{
    editor_init(state, hub_context);
}

void on_reload(Editor_State *state)
{
}

void on_frame(Editor_State *state, const struct Hub_Timing *t)
{
    editor_frame(state, t);
}

void on_platform_event(Editor_State *state, const struct Hub_Event *e)
{
    editor_event(state, e);
}

void on_destroy(Editor_State *state)
{
    editor_destroy(state);
}
