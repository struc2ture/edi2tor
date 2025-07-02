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

#include "../src/editor.h"

void on_run(Text_Buffer *output_buffer, Editor_State *state)
{
    Cursor_Pos pos = {0, 0};
    for (int i = 0; i < 10; i++)
    {
        pos = text_buffer_insert_range(output_buffer, "Hello world ", pos);
        pos = text_buffer_insert_range(output_buffer, "HAHA\n", pos);
    }
}

#include "../src/editor.h"
#include "../src/util.h"

void on_run(Text_Buffer *ob, Editor_State *s)
{
    char *src_name = strf("prog_%ld", time(NULL));
    char *src_path = strf("scratch/%s.c", src_name);
    char *bin_path = strf("scratch/%s", src_name);

    file_write(s->views[1]->bv.buffer->text_buffer, src_path);

    const char *cc = "clang";
    // const char *cflags = "-I/opt/homebrew/include -isystemthird_party -DGL_SILENCE_DEPRECATION";
    // const char *lflags = "-L/opt/homebrew/lib -lglfw -framework OpenGL";
    char *compile_command = strf("%s %s -o %s", cc, src_path, bin_path);

    trace_log("%s", compile_command);

    int result = system(compile_command);

    file_delete(src_path);

    free(bin_path);
    free(src_path);
    free(src_name);
    free(compile_command);
}
#endif
