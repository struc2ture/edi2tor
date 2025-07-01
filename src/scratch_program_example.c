#if 0
#include "../src/editor.h"

void on_run(Text_Buffer *output_buffer, Editor_State *state)
{
    Cursor_Pos pos = {0};
    for (int i = 0; i < 16; i++)
    {
        for (int j = 0; j < i + 1; j++)
        {
            text_buffer_insert_char(output_buffer, '$', pos);
            pos = cursor_pos_advance_char(*output_buffer, pos, +1, true);
        }

        text_buffer_insert_char(output_buffer, '\n', pos);
        pos = cursor_pos_advance_char(*output_buffer, pos, +1, true);
    }
}

#include "../src/editor.h"

void on_run(Text_Buffer *output_buffer, Editor_State *state)
{
    state->render_state.text_color = (Color){255, 255, 255, 255};
    text_buffer_insert_range(output_buffer, "HAHAHA", (Cursor_Pos){0, 0});

    Text_Buffer test_buffer = text_buffer_create_from_lines("HELLO WORLD", NULL);
    create_buffer_view_generic(test_buffer, (Rect){0, 0, 200, 70}, state);
}
#endif
