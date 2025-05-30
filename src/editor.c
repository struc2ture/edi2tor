#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_easy_font.h>

#include "editor.h"

#include "util.h"

const char *vs_src =
"#version 410 core\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec2 aTexCoord;\n"
"layout (location = 2) in vec4 aColor;\n"
"uniform mat4 u_mvp;\n"
"out vec2 TexCoord;\n"
"out vec4 Color;\n"
"void main() {\n"
"    gl_Position = u_mvp * vec4(aPos, 0.0, 1.0);\n"
"    TexCoord = aTexCoord;\n"
"    Color = aColor;\n"
"}";

const char *fs_src =
"#version 410 core\n"
"out vec4 FragColor;\n"
"in vec2 TexCoord;\n"
"uniform sampler2D u_tex;\n"
"in vec4 Color;\n"
"void main() {\n"
"    FragColor = vec4(vec3(Color), Color.a * texture(u_tex, TexCoord).r);\n"
"}";

void _init(GLFWwindow *window, void *_state)
{
    (void)window; Editor_State *state = _state; (void)state;
    bassert(sizeof(*state) < 4096);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int framebuffer_w, framebuffer_h;
    glfwGetFramebufferSize(window, &framebuffer_w, &framebuffer_h);
    glViewport(0, 0, framebuffer_w, framebuffer_h);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, 0);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, 0);
    glCompileShader(fs);
    { // TODO Move to a function
        GLuint shader = fs;
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), NULL, log);
            fprintf(stderr, "Shader compile error:\n%s\nSource:\n", log);
            fprintf(stderr, "%s\n", fs_src);
        }
    }
    state->prog = glCreateProgram();
    glAttachShader(state->prog, vs);
    glAttachShader(state->prog, fs);
    glLinkProgram(state->prog);
    { // TODO Move to a function
        GLint success = 0;
        glGetProgramiv(state->prog, GL_LINK_STATUS, &success);
        if (!success) {
            char log[512];
            glGetProgramInfoLog(state->prog, sizeof(log), NULL, log);
            fprintf(stderr, "Program link error:\n%s\n", log);
        }
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    glUseProgram(state->prog);

    glGenVertexArrays(1, &state->vao);
    glGenBuffers(1, &state->vbo);
    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER,  VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (void *)offsetof(Vert, r));
    glEnableVertexAttribArray(2);
    glfwSetWindowUserPointer(window, _state);

    state->shader_mvp_loc = glGetUniformLocation(state->prog, "u_mvp");
    state->viewport.zoom = DEFAULT_ZOOM;
    int window_w, window_h;
    glfwGetWindowSize(window, &window_w, &window_h);
    state->viewport.window_dim.x = window_w;
    state->viewport.window_dim.y = window_h;
    update_shader_mvp(state);

    open_file_for_edit(FILE_PATH, state);

    state->font = load_font(FONT_PATH);
}

void _hotreload_init(GLFWwindow *window)
{
    (void)window;
    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetWindowRefreshCallback(window, refresh_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
}

void _render(GLFWwindow *window, void *_state)
{
    (void)window; Editor_State *state = _state; (void)state;

    if (state->should_break) {
        __builtin_debugtrap();
        state->should_break = false;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    draw_text_buffer(state);
    draw_cursor(state);
    draw_selection(state);

    handle_mouse_input(window, state);

    glfwSwapBuffers(window);
    glfwPollEvents();

    state->frame_count++;
    state->cursor.frame_count++;
}

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window);
    if (codepoint < 128) {
        // TODO Refactor to use some kind centralized char stream into different destinations logic
        if (!state->go_to_line_mode) {
            if (is_selection_valid(state))
            {
                delete_selected(state);
            }
            insert_char(&state->text_buffer, (char)codepoint, &state->cursor, state, false);
        } else {
            insert_go_to_line_char(state, (char)codepoint);
        }
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods; Editor_State *state = glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, 1);
    }
    else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (mods & GLFW_MOD_SHIFT && !is_selection_valid(state))
        {
            start_selection_at_cursor(state);
        }
        if (mods & GLFW_MOD_ALT)
        {
            move_cursor_to_next_white_line(state);
        }
        else if (mods & GLFW_MOD_SUPER)
        {
            move_cursor_to_line(&state->text_buffer, &state->cursor, state, MAX_LINES, true);
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, MAX_CHARS_PER_LINE, true, false);
        }
        else
        {
            move_cursor_to_line(&state->text_buffer, &state->cursor, state, state->cursor.pos.l + 1, true);
        }
        if (mods & GLFW_MOD_SHIFT)
        {
            extend_selection_to_cursor(state);
        }
        else
        {
            cancel_selection(state);
        }
    }
    else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (mods & GLFW_MOD_SHIFT && !is_selection_valid(state))
        {
            start_selection_at_cursor(state);
        }
        if (mods & GLFW_MOD_ALT)
        {
            move_cursor_to_prev_white_line(state);
        }
        else if (mods & GLFW_MOD_SUPER)
        {
            move_cursor_to_line(&state->text_buffer, &state->cursor, state, 0, true);
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, true, false);
        }
        else
        {
            move_cursor_to_line(&state->text_buffer, &state->cursor, state, state->cursor.pos.l - 1, true);
        }
        if (mods & GLFW_MOD_SHIFT)
        {
            extend_selection_to_cursor(state);
        }
        else
        {
            cancel_selection(state);
        }
    }
    else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (mods & GLFW_MOD_SHIFT && !is_selection_valid(state))
        {
            start_selection_at_cursor(state);
        }
        if (mods & GLFW_MOD_ALT)
        {
            move_cursor_to_prev_start_of_word(state);
        }
        else if (mods & GLFW_MOD_SUPER)
        {
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, true, false);
        }
        else
        {
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, state->cursor.pos.c - 1, true, true);
        }
        if (mods & GLFW_MOD_SHIFT)
        {
            extend_selection_to_cursor(state);
        }
        else
        {
            cancel_selection(state);
        }
    }
    else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (mods & GLFW_MOD_SHIFT && !is_selection_valid(state))
        {
            start_selection_at_cursor(state);
        }
        if (mods & GLFW_MOD_ALT)
        {
            move_cursor_to_next_end_of_word(state);
        }
        else if (mods & GLFW_MOD_SUPER)
        {
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, MAX_CHARS_PER_LINE, true, false);
        }
        else
        {
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, state->cursor.pos.c + 1, true, true);
        }
        if (mods & GLFW_MOD_SHIFT)
        {
            extend_selection_to_cursor(state);
        }
        else
        {
            cancel_selection(state);
        }
    }
    else if (key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (!state->go_to_line_mode)
        {
            insert_char(&state->text_buffer, '\n', &state->cursor, state, true);
        }
        else
        {
            char *end;
            int go_to_line = strtol(state->go_to_line_chars, &end, 10);
            if (*end == '\0')
            {
                move_cursor_to_line(&state->text_buffer, &state->cursor, state, go_to_line - 1, true);
            }
            state->go_to_line_mode = false;
            memset(state->go_to_line_chars, 0, GO_TO_LINE_CHAR_MAX);
            state->current_go_to_line_char_i = 0;
        }
    }
    else if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (!state->go_to_line_mode)
        {
            if (is_selection_valid(state))
            {
                delete_selected(state);
            }
            else
            {
                remove_char(&state->text_buffer, &state->cursor, state);
            }
        }
    }
    else if (key == GLFW_KEY_TAB && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (!state->go_to_line_mode)
        {
            if (mods == GLFW_MOD_ALT)
            {
                int indent_i = get_line_indent(state->text_buffer.lines[state->cursor.pos.l]);
                move_cursor_to_char(&state->text_buffer, &state->cursor, state, indent_i, true, false);
            }
            else
            {
                insert_indent(&state->text_buffer, &state->cursor, state);
            }
        }
    }
    else if (key == GLFW_KEY_LEFT_BRACKET && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (!state->go_to_line_mode)
        {
            decrease_indent_level(&state->text_buffer, &state->cursor, state);
        }
    }
    else if (key == GLFW_KEY_RIGHT_BRACKET && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        if (!state->go_to_line_mode)
        {
            increase_indent_level(&state->text_buffer, &state->cursor, state);
        }
    }
    else if (key == GLFW_KEY_C && mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
    {
        copy_at_selection(state);
    }
    else if (key == GLFW_KEY_V && mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
    {
        paste_from_copy_buffer(state);
    }
    else if (key == GLFW_KEY_X && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        delete_current_line(state);
    }

    else if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
    {
        state->debug_invis = !state->debug_invis;
    }
    else if (key == GLFW_KEY_F12 && action == GLFW_PRESS)
    {
        state->should_break = true;
    }
    else if (key ==GLFW_KEY_F9 && action == GLFW_PRESS)
    {
        rebuild_dl();
    }
    else if (key ==GLFW_KEY_F8 && action == GLFW_PRESS)
    {
        validate_text_buffer(&state->text_buffer);
        trace_log("Validated text buffer");
    }
    else if (key ==GLFW_KEY_F7 && action == GLFW_PRESS)
    {
        open_file_for_edit(FILE_PATH, state);
    }
    else if (key == GLFW_KEY_S && mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
    {
        write_file(state->text_buffer, state->file_info);
    }
    else if (key == GLFW_KEY_EQUAL && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        state->viewport.zoom += 0.25f;
        update_shader_mvp(state);
    }
    else if (key == GLFW_KEY_MINUS && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        state->viewport.zoom -= 0.25f;
        update_shader_mvp(state);
    }
    else if (key == GLFW_KEY_G && mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
    {
        state->go_to_line_mode = !state->go_to_line_mode;
        if (!state->go_to_line_mode)
        {
            memset(state->go_to_line_chars, 0, GO_TO_LINE_CHAR_MAX);
            state->current_go_to_line_char_i = 0;
        }
    }
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void)window; (void)button; (void)action; (void)mods; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
}

void scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window; (void)x_offset; (void)y_offset; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    state->viewport.offset.x -= x_offset * SCROLL_SENS;
    state->viewport.offset.y -= y_offset * SCROLL_SENS;
    update_shader_mvp(state);
}

void framebuffer_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window; (void)w; (void)h;
    glViewport(0, 0, w, h);
}

void window_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window; (void)w; (void)h; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    state->viewport.window_dim.x = w;
    state->viewport.window_dim.y = h;
    update_shader_mvp(state);
}

void refresh_callback(GLFWwindow* window) {
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    _render(window, state);
}

void handle_mouse_input(GLFWwindow *window, Editor_State *state)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        bool is_just_pressed = !state->left_mouse_down;
        bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        if (is_shift_pressed && is_just_pressed && !is_selection_valid(state)) {
            start_selection_at_cursor(state);
        }
        Buf_Pos bp = get_buf_pos_under_mouse(window, state);
        move_cursor_to_line(&state->text_buffer, &state->cursor, state, bp.l, false);
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, bp.c, false, false);
        extend_selection_to_cursor(state);
        if (!is_shift_pressed && is_just_pressed) {
            start_selection_at_cursor(state);
        }
        if (is_shift_pressed || !is_just_pressed) {
            extend_selection_to_cursor(state);
        }
        state->left_mouse_down = true;
    } else {
        state->left_mouse_down = false;
    }
}

Vert make_vert(float x, float y, float u, float v, unsigned char color[4])
{
    Vert vert = {x, y, u, v, color[0], color[1], color[2], color[3]};
    return vert;
}

void vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert)
{
    bassert(vert_buffer->vert_count < VERT_MAX);
    vert_buffer->verts[vert_buffer->vert_count++] = vert;
}

Render_Font load_font(const char *path)
{
    Render_Font font = {0};

    FILE *f = fopen(path, "rb");
    if (!f) fatal("Failed to open file for reading at %s", path);
    fseek(f, 0, SEEK_END);
    int file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *file_bytes = xmalloc(file_size);
    fread(file_bytes, 1, file_size, f);

    void *atlas_bitmap = xcalloc(512 * 512);

    font.size = FONT_SIZE;
    font.atlas_w = 512;
    font.atlas_h = 512;
    font.char_count = 96;
    font.char_data = xcalloc(font.char_count * sizeof(font.char_data[0]));
    stbtt_BakeFontBitmap(file_bytes, 0, font.size, atlas_bitmap, font.atlas_w, font.atlas_h, 32, font.char_count, font.char_data);
    stbtt_GetScaledFontVMetrics(file_bytes, 0, font.size, &font.ascent, &font.descent, &font.line_gap);
    free(file_bytes);

    glGenTextures(1, &font.white_texture);
    glBindTexture(GL_TEXTURE_2D, font.white_texture);
    unsigned char white_texture_bytes[] = { 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, white_texture_bytes);

    glGenTextures(1, &font.texture);
    glBindTexture(GL_TEXTURE_2D, font.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.atlas_w, font.atlas_h, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    free(atlas_bitmap);

    return font;
}

float get_font_line_height(Render_Font *font)
{
    float height = font->ascent - font->descent;
    return height;
}

float get_char_width(char c, Render_Font *font)
{
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    stbtt_GetBakedQuad(font->char_data, font->atlas_w, font->atlas_h, c-32, &x, &y ,&q, 1);
    return x;
}

Rect_Bounds get_string_rect(const char *str, Render_Font *font, float x, float y)
{
    Rect_Bounds r;
    r.min.x = x;
    r.min.y = y;

    while (*str)
    {
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->char_data, font->atlas_w, font->atlas_h, *str-32, &x, &y ,&q, 1);
        str++;
    }

    r.max.x = x;
    r.max.y = r.min.y + get_font_line_height(font);
    return r;
}

Rect_Bounds get_string_range_rect(const char *str, Render_Font *font, int start_char, int end_char, bool include_new_line_char)
{
    bassert(start_char >= 0);
    bassert(start_char < end_char);
    Rect_Bounds r = {0};
    r.min.y = 0;
    r.max.y = get_font_line_height(font);
    float x = 0, y = 0;
    for (int i = 0; i < end_char && str[i]; i++)
    {
        // Capture the x before stbtt_GetBakedQuad advances x past the starting char
        if (i == start_char)
        {
            r.min.x = x;
        }

        char c = str[i];
        if (include_new_line_char && c == '\n')
        {
            c = ' ';
        }
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->char_data, font->atlas_w, font->atlas_h, c - 32, &x, &y, &q, 1);
    }

    // If reached end_char index, x will be max_x (end_char itself is not included)
    // If reached null terminator, it will still be valid, highlighting the whole string,
    // stbtt_GetBakedQuad setting x past the advance of the last char
    r.max.x = x;
    return r;
}

Rect_Bounds get_string_char_rect(const char *str, Render_Font *font, int char_i)
{
    Rect_Bounds r = {0};
    r.max.y = get_font_line_height(font);
    float x = 0, y = 0;
    int i = 0;
    bool found_max = false;
    while (*str)
    {
        char c = *str;
        if (c == '\n')
        {
            c = ' ';
        }
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->char_data, font->atlas_w, font->atlas_h, c - 32, &x, &y ,&q, 1);
        if (i == char_i)
        {
            r.max.x = x;
            found_max = true;
            break;
        } else
        {
            r.min.x = x;
        }
        str++;
        i++;
    }
    bassert(found_max);
    return r;
}

int get_char_i_at_pos_in_string(const char *str, Render_Font *font, float x)
{
    int char_i = 0;
    float str_x = 0, str_y = 0;
    while (*str)
    {
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->char_data, font->atlas_w, font->atlas_h, *str - 32, &str_x, &str_y ,&q, 1);
        if (str_x > x)
        {
            return char_i;
        }
        char_i++;
        str++;
    }
    return char_i;
}

Rect_Bounds get_cursor_rect(Editor_State *state)
{
    Rect_Bounds cursor_rect = get_string_char_rect(state->text_buffer.lines[state->cursor.pos.l].str, &state->font, state->cursor.pos.c);
    float line_height = get_font_line_height(&state->font);
    float y = state->cursor.pos.l * line_height;
    cursor_rect.min.y += y;
    cursor_rect.max.y += y;
    return cursor_rect;
}

void draw_string(const char *str, Render_Font *font, float x, float y, unsigned char color[4])
{
    y += font->ascent;
    Vert_Buffer vert_buf = {0};
    while (*str)
    {
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->char_data, font->atlas_w, font->atlas_h, *str-32, &x, &y ,&q, 1);
        vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y0, q.s0, q.t0, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y1, q.s1, q.t1, color));
        str++;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, font->white_texture);
}


void draw_quad(float x, float y, float width, float height, unsigned char color[4])
{
    Vert_Buffer vert_buf = {0};
    vert_buf.vert_count = 6;
    vert_buf.verts[0].x = 0;     vert_buf.verts[0].y = 0;
    vert_buf.verts[1].x = 0;     vert_buf.verts[1].y = height;
    vert_buf.verts[2].x = width; vert_buf.verts[2].y = 0;
    vert_buf.verts[3].x = width; vert_buf.verts[3].y = 0;
    vert_buf.verts[4].x = 0;     vert_buf.verts[4].y = height;
    vert_buf.verts[5].x = width; vert_buf.verts[5].y = height;
    for (int i = 0; i < 6; i++) {
        vert_buf.verts[i].x += x;
        vert_buf.verts[i].y += y;
        vert_buf.verts[i].r = color[0];
        vert_buf.verts[i].g = color[1];
        vert_buf.verts[i].b = color[2];
        vert_buf.verts[i].a = color[3];
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_text_buffer(Editor_State *state)
{
    float y = 0;
    float line_height = get_font_line_height(&state->font);

    for (int i = 0; i < state->text_buffer.line_count; i++)
    {
        Rect_Bounds string_rect = get_string_rect(state->text_buffer.lines[i].str, &state->font, 0, y);
        bool is_seen = is_canvas_rect_in_viewport(state->viewport, string_rect);
        if (is_seen)
        {
            // draw_quad(string_rect.min.x,
            //     string_rect.min.y,
            //     string_rect.max.x - string_rect.min.x,
            //     string_rect.max.y - string_rect.min.y,
            //     (unsigned char[]){255, 255, 255, 255});
            draw_string(state->text_buffer.lines[i].str, &state->font, 0, y, (unsigned char[]){255, 255, 255, 255});
        }
        y += line_height;
    }
}

void draw_cursor(Editor_State *state)
{
    if (!((state->cursor.frame_count % (CURSOR_BLINK_ON_FRAMES * 2)) / CURSOR_BLINK_ON_FRAMES))
    {
        Rect_Bounds cursor_rect = get_cursor_rect(state);

        bool is_seen = is_canvas_rect_in_viewport(state->viewport, cursor_rect);
        if (is_seen)
        {
            draw_quad(cursor_rect.min.x,
                cursor_rect.min.y,
                cursor_rect.max.x - cursor_rect.min.x,
                cursor_rect.max.y - cursor_rect.min.y,
                (unsigned char[]){0, 0, 255, 255});
        }
    }
}

void draw_selection(Editor_State *state)
{
    if (is_selection_valid(state)) {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l || (start.l == end.l && start.c > end.c)) {
            Buf_Pos tmp = start;
            start = end;
            end = tmp;
        }
        for (int i = start.l; i <= end.l; i++)
        {
            Text_Line *line = &state->text_buffer.lines[i];
            int h_start, h_end;
            bool extend_to_new_line_char = false;
            if (i == start.l && i == end.l) {
                h_start = start.c;
                h_end = end.c;
            } else if (i == start.l) {
                h_start = start.c;
                h_end = line->len;
                extend_to_new_line_char = true;
            } else if (i == end.l) {
                h_start = 0;
                h_end = end.c;
            } else {
                h_start = 0;
                h_end = line->len;
                extend_to_new_line_char = true;
            }
            if (h_end > h_start)
            {
                Rect_Bounds rect = get_string_range_rect(line->str, &state->font, h_start, h_end, true);

                rect.min.y += i * get_font_line_height(&state->font);
                rect.max.y += i * get_font_line_height(&state->font);

                (void)extend_to_new_line_char;

                if (is_canvas_rect_in_viewport(state->viewport, rect))
                {
                    unsigned char color[] = { 200, 200, 200, 130 };
                    draw_quad(rect.min.x, rect.min.y, rect.max.x - rect.min.x, rect.max.y - rect.min.y, color);
                }
            }
        }
    }
}

void render_old(GLFWwindow *window, Editor_State *state)
{
    int line_height = LINE_HEIGHT;
    int x_line_num_offset = stb_easy_font_width("000: ");

    int lines_rendered = 0;

    int x_offset = 0;
    int y_offset = 0;
    for (int line_i = 0; line_i < state->text_buffer.line_count; line_i++) {
        bool line_min_in_bounds = is_canvas_y_pos_in_bounds(line_i * line_height, state->viewport);
        bool line_max_in_bounds = is_canvas_y_pos_in_bounds((line_i + 1) * line_height, state->viewport);
        if (line_min_in_bounds || line_max_in_bounds) {
            unsigned char color[] = { 240, 240, 240, 255 };
            if (line_i == state->cursor.pos.l) {
                color[0] = 140;
                color[1] = 140;
                color[2] = 240;
                color[3] = 255;
            }

            char line_i_str_buf[256];
            snprintf(line_i_str_buf, sizeof(line_i_str_buf), "%3d: ", line_i + 1);
            unsigned char line_num_color[] = { 140, 140, 140, 255 };
            draw_string_old(x_offset, y_offset, line_i_str_buf, line_num_color);
            x_offset += x_line_num_offset;

            draw_content_string(x_offset, y_offset, state->text_buffer.lines[line_i].str, color, state);

            x_offset = 0;
            lines_rendered++;
        }

        y_offset += line_height;
    }

    if (!((state->cursor.frame_count % (CURSOR_BLINK_ON_FRAMES * 2)) / CURSOR_BLINK_ON_FRAMES))
    {
        int cursor_x_offset = stb_easy_font_width_up_to(state->text_buffer.lines[state->cursor.pos.l].str, state->cursor.pos.c);
        int cursor_x = x_line_num_offset + cursor_x_offset;
        int cursor_y = line_height * state->cursor.pos.l;
        int cursor_width = stb_easy_font_char_width(state->text_buffer.lines[state->cursor.pos.l].str[state->cursor.pos.c]);
        if (cursor_width == 0) cursor_width = 6;
        int cursor_height = 12;
        unsigned char color[] = { 200, 200, 200, 255 };
        draw_quad(cursor_x, cursor_y, cursor_width, cursor_height, color);
    }

    if (is_selection_valid(state)) {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l) {
            Buf_Pos tmp = start;
            start = end;
            end = tmp;
        }
        for (int i = start.l; i <= end.l; i++) {
            Text_Line *line = &state->text_buffer.lines[i];
            int h_start, h_end;
            bool extend_to_new_line_char = false;
            if (i == start.l && i == end.l) {
                h_start = start.c;
                h_end = end.c;
                if (h_start > h_end) {
                    int tmp = h_start;
                    h_start = h_end;
                    h_end = tmp;
                }
            } else if (i == start.l) {
                h_start = start.c;
                h_end = line->len - 1;
                extend_to_new_line_char = true;
            } else if (i == end.l) {
                h_start = 0;
                h_end = end.c;
            } else {
                h_start = 0;
                h_end = line->len - 1;
                extend_to_new_line_char = true;
            }
            int sel_x = x_line_num_offset + stb_easy_font_width_up_to(line->str, h_start);
            int sel_y = line_height * i;
            int sel_w = x_line_num_offset + stb_easy_font_width_up_to(line->str, h_end) - sel_x;
            if (extend_to_new_line_char) sel_w += 6;
            int sel_h = 12;
            unsigned char color[] = { 200, 200, 200, 130 };
            draw_quad(sel_x, sel_y, sel_w, sel_h, color);
        }
    }

    if (ENABLE_STATUS_BAR) {
        Vec_2 viewport_dim = get_viewport_dim(state->viewport);
        unsigned char bg_color[] = { 30, 30, 30, 255 };
        draw_quad(state->viewport.offset.x, state->viewport.offset.y + viewport_dim.y - 2 * line_height - 3, viewport_dim.x, 3 + 2 * line_height, bg_color);

        char line_i_str_buf[256];
        unsigned char color[] = { 200, 200, 200, 255 };

        // TODO Make this not ugly
        if (!state->go_to_line_mode) {
            snprintf(line_i_str_buf, sizeof(line_i_str_buf),
                "STATUS: Cursor: %d, %d; Line Len: %d; Invis: %d; Lines: %d; Rendered: %d",
                state->cursor.pos.l,
                state->cursor.pos.c,
                state->text_buffer.lines[state->cursor.pos.l].len,
                state->debug_invis,
                state->text_buffer.line_count,
                lines_rendered);
            draw_string_old(state->viewport.offset.x + 10, state->viewport.offset.y + viewport_dim.y - 2 * line_height, line_i_str_buf, color);
        } else {
            snprintf(line_i_str_buf, sizeof(line_i_str_buf),
                "STATUS: Cursor: %d, %d; Line Len: %d; Invis: %d; Lines: %d; Rendered: %d; Go to: %s",
                state->cursor.pos.l,
                state->cursor.pos.c,
                state->text_buffer.lines[state->cursor.pos.l].len,
                state->debug_invis,
                state->text_buffer.line_count,
                lines_rendered,
                state->go_to_line_chars);
            draw_string_old(state->viewport.offset.x + 10, state->viewport.offset.y + viewport_dim.y - 2 * line_height, line_i_str_buf, color);
        }

        Rect_Bounds viewport_bounds = get_viewport_bounds(state->viewport);
        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        Vec_2 mouse_window_pos = { .x = mouse_x, .y = mouse_y };
        Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state->viewport);

        snprintf(line_i_str_buf, sizeof(line_i_str_buf),
            "Viewport bounds: " VEC2_FMT VEC2_FMT "; Zoom: %0.2f; Mouse Position: window: " VEC2_FMT " canvas: " VEC2_FMT,
            VEC2_ARG(viewport_bounds.min),
            VEC2_ARG(viewport_bounds.max),
            state->viewport.zoom,
            VEC2_ARG(mouse_window_pos),
            VEC2_ARG(mouse_canvas_pos));
        draw_string_old(state->viewport.offset.x + 10, state->viewport.offset.y + viewport_dim.y - line_height, line_i_str_buf, color);
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        bool is_just_pressed = !state->left_mouse_down;
        bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        if (is_shift_pressed && is_just_pressed && !is_selection_valid(state)) {
            start_selection_at_cursor(state);
        }
        Buf_Pos bp = get_buf_pos_under_mouse(window, state);
        move_cursor_to_line(&state->text_buffer, &state->cursor, state, bp.l, false);
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, bp.c, false, false);
        extend_selection_to_cursor(state);
        if (!is_shift_pressed && is_just_pressed) {
            start_selection_at_cursor(state);
        }
        if (is_shift_pressed || !is_just_pressed) {
            extend_selection_to_cursor(state);
        }
        state->left_mouse_down = true;
    } else {
        state->left_mouse_down = false;
    }
}

void render_string(int x, int y, char *line, unsigned char color[4], Vert_Buffer *out_vert_buf)
{
    (void)color;
    char quad_buf[99999];
    int quad_count = stb_easy_font_print(x, y, line, NULL, quad_buf, sizeof(quad_buf));
    out_vert_buf->vert_count = quad_count * 6;
    {
        float *src = (float *)quad_buf;
        for (int i = 0; i < quad_count; ++i) {
            float *q = &src[i * 4 * 4];
            Vert *dst = &out_vert_buf->verts[i * 6];
            dst[0].x = q[0];  dst[0].y = q[1];
            dst[1].x = q[4];  dst[1].y = q[5];
            dst[2].x = q[8];  dst[2].y = q[9];
            dst[3].x = q[0];  dst[3].y = q[1];
            dst[4].x = q[8];  dst[4].y = q[9];
            dst[5].x = q[12]; dst[5].y = q[13];
            for (int i = 0; i < 6; i++) {
                dst[i].r = color[0];
                dst[i].g = color[1];
                dst[i].b = color[2];
                dst[i].a = color[3];
            }
        }
    }
}


void draw_string_old(int x, int y, char *string, unsigned char color[4])
{
    Vert_Buffer vert_buf = {0};
    render_string(x, y, string, color, &vert_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_content_string(int x, int y, char *string, unsigned char color[4], Editor_State *state)
{
    if (state->debug_invis) {
        string = xstrdup(string);
        convert_to_debug_invis(string);
    }
    draw_string_old(x, y, string, color);
    if (state->debug_invis) {
        free(string);
    }
}

void make_ortho(float left, float right, float bottom, float top, float near, float far, float *out)
{
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;

    out[0]  = 2.0f / rl;  out[1]  = 0;         out[2]  = 0;          out[3]  = 0;
    out[4]  = 0;          out[5]  = 2.0f / tb; out[6]  = 0;          out[7]  = 0;
    out[8]  = 0;          out[9]  = 0;         out[10] = -2.0f / fn; out[11] = 0;
    out[12] = -(right + left) / rl;
    out[13] = -(top + bottom) / tb;
    out[14] = -(far + near) / fn;
    out[15] = 1.0f;
}

void make_view(float offset_x, float offset_y, float scale, float *out)
{
    out[0]  = scale; out[1]  = 0;     out[2]  = 0; out[3]  = 0;
    out[4]  = 0;     out[5]  = scale; out[6]  = 0; out[7]  = 0;
    out[8]  = 0;     out[9]  = 0;     out[10] = 1; out[11] = 0;
    out[12] = -offset_x * scale;
    out[13] = -offset_y * scale;
    out[14] = 0;
    out[15] = 1;
}

void mul_mat4(const float *a, const float *b, float *out) // row-major
{
    for (int row = 0; row < 4; row++)
    for (int col = 0; col < 4; col++) {
        out[col + row * 4] = 0.0f;
        for (int k = 0; k < 4; k++) {
            out[col + row * 4] += a[k + row * 4] * b[col + k * 4];
        }
    }
}

Rect_Bounds get_viewport_bounds(Viewport viewport)
{
    Rect_Bounds r = {0};
    r.min.x = viewport.offset.x;
    r.min.y = viewport.offset.y;
    r.max.x = r.min.x + viewport.window_dim.x / viewport.zoom;
    r.max.y = r.min.y + viewport.window_dim.y / viewport.zoom;
    return r;
}

Rect_Bounds get_viewport_cursor_bounds(Viewport viewport, Render_Font *font)
{
    Rect_Bounds viewport_bounds = get_viewport_bounds(viewport);
    float space_width = get_char_width(' ', font);
    float font_line_height = get_font_line_height(font);
    Rect_Bounds viewport_cursor_bounds = {0};
    viewport_cursor_bounds.min.x = viewport_bounds.min.x + VIEWPORT_CURSOR_BOUNDARY_COLUMNS * space_width;
    viewport_cursor_bounds.max.x = viewport_bounds.max.x - VIEWPORT_CURSOR_BOUNDARY_COLUMNS * space_width;
    viewport_cursor_bounds.min.y = viewport_bounds.min.y + VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
    viewport_cursor_bounds.max.y = viewport_bounds.max.y - VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
    return viewport_cursor_bounds;
}

Vec_2 get_viewport_dim(Viewport viewport)
{
    Vec_2 r = {0};
    r.x = viewport.window_dim.x / viewport.zoom;
    r.y = viewport.window_dim.y / viewport.zoom;
    return r;
}

Vec_2 window_pos_to_canvas_pos(Vec_2 window_pos, Viewport viewport)
{
    Vec_2 r = {0};
    r.x = viewport.offset.x + window_pos.x / viewport.zoom;
    r.y = viewport.offset.y + window_pos.y / viewport.zoom;
    return r;
}

bool is_canvas_pos_in_bounds(Vec_2 canvas_pos, Viewport viewport)
{
    Rect_Bounds viewport_bounds = get_viewport_bounds(viewport);
    return (canvas_pos.x > viewport_bounds.min.x &&
            canvas_pos.x < viewport_bounds.max.x &&
            canvas_pos.y > viewport_bounds.min.y &&
            canvas_pos.y < viewport_bounds.max.y);
}

bool is_canvas_y_pos_in_bounds(float canvas_y, Viewport viewport)
{
    Rect_Bounds viewport_bounds = get_viewport_bounds(viewport);
    return (canvas_y > viewport_bounds.min.y &&
            canvas_y < viewport_bounds.max.y);
}

Vec_2 get_mouse_canvas_pos(GLFWwindow *window, Viewport viewport)
{
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    Vec_2 mouse_window_pos = { .x = mouse_x, .y = mouse_y };
    Vec_2 mouse_canvas_pos = window_pos_to_canvas_pos(mouse_window_pos, viewport);
    return mouse_canvas_pos;
}

Buf_Pos get_buf_pos_under_mouse(GLFWwindow *window, Editor_State *state)
{
    Buf_Pos r;
    Vec_2 pos = get_mouse_canvas_pos(window, state->viewport);
    r.l = pos.y / (float)get_font_line_height(&state->font);
    r.c = 0;
    if (r.l < 0) {
        r.l = 0;
    } else if (r.l >= state->text_buffer.line_count) {
        r.l = state->text_buffer.line_count - 1;
    }
    r.c = get_char_i_at_pos_in_string(state->text_buffer.lines[r.l].str, &state->font, pos.x);
    return r;
}

void update_shader_mvp(Editor_State *state)
{
    make_ortho(0, state->viewport.window_dim.x, state->viewport.window_dim.y, 0, -1, 1, state->proj_transform);
    make_view(state->viewport.offset.x, state->viewport.offset.y, state->viewport.zoom, state->view_transform);
    float mvp[16];
    mul_mat4(state->view_transform, state->proj_transform, mvp);
    glUniformMatrix4fv(state->shader_mvp_loc, 1, GL_FALSE, mvp);
}

void viewport_snap_to_cursor(Editor_State *state)
{
    Viewport *viewport = &state->viewport;
    Rect_Bounds viewport_cursor_bounds = get_viewport_cursor_bounds(*viewport, &state->font);
    Rect_Bounds cursor_rect = get_cursor_rect(state);
    float font_space_width = get_char_width(' ', &state->font);
    float font_line_height = get_font_line_height(&state->font);
    bool will_update_shader = false;
    if (cursor_rect.max.y <= viewport_cursor_bounds.min.y || cursor_rect.min.y >= viewport_cursor_bounds.max.y)
    {
        Vec_2 viewport_dim = get_viewport_dim(*viewport);
        if (cursor_rect.max.y <= viewport_cursor_bounds.min.y)
        {
            viewport->offset.y = cursor_rect.min.y - VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
        }
        else
        {
            viewport->offset.y = cursor_rect.max.y - viewport_dim.y + VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
        }
        will_update_shader = true;
    }
    if (cursor_rect.max.x <= viewport_cursor_bounds.min.x || cursor_rect.min.x >= viewport_cursor_bounds.max.x)
    {
        Vec_2 viewport_dim = get_viewport_dim(*viewport);
        if (cursor_rect.max.x <= viewport_cursor_bounds.min.x)
        {
            viewport->offset.x = cursor_rect.min.x - VIEWPORT_CURSOR_BOUNDARY_COLUMNS * font_space_width;
            if (viewport->offset.x < 0.0f)
            {
                viewport->offset.x = 0.0f;
            }
        }
        else
        {
            viewport->offset.x = cursor_rect.max.x - viewport_dim.x + VIEWPORT_CURSOR_BOUNDARY_COLUMNS * font_space_width;
        }
        will_update_shader = true;
    }
    if (will_update_shader)
    {
        update_shader_mvp(state);
    }
}

bool is_canvas_rect_in_viewport(Viewport viewport, Rect_Bounds rect)
{
    Rect_Bounds viewport_bounds = get_viewport_bounds(viewport);
    bool intersect = rect.min.x < viewport_bounds.max.x && rect.max.x > viewport_bounds.min.x &&
        rect.min.y < viewport_bounds.max.y && rect.max.y > viewport_bounds.min.y;
    return intersect;
}

bool is_buf_pos_valid(const Text_Buffer *tb, Buf_Pos bp)
{
    bool is_valid = bp.l >= 0 && bp.l < tb->line_count &&
        bp.c >= 0 && bp.c < tb->lines[bp.l].len;
    return is_valid;
}

bool is_buf_pos_equal(Buf_Pos a, Buf_Pos b)
{
    return a.l == b.l && a.c == b.c;
}

void move_cursor_to_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state, int to_line, bool snap_viewport)
{
    int orig_cursor_line = cursor->pos.l;
    cursor->pos.l = to_line;
    if (cursor->pos.l < 0)
    {
        cursor->pos.l = 0;
        cursor->pos.c = 0;
    }
    if (cursor->pos.l >= text_buffer->line_count)
    {
        cursor->pos.l = text_buffer->line_count - 1;
        cursor->pos.c = text_buffer->lines[cursor->pos.l].len - 1;
    }
    if (cursor->pos.l != orig_cursor_line)
    {
        if (cursor->pos.c >= text_buffer->lines[cursor->pos.l].len)
        {
            cursor->pos.c = text_buffer->lines[cursor->pos.l].len - 1;
        }
        cursor->frame_count = 0;
        if (snap_viewport)
        {
            viewport_snap_to_cursor(state);
        }
    }
}

void move_cursor_to_char(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state, int to_char, bool snap_viewport, bool can_switch_line)
{
    int prev_cursor_char = cursor->pos.c;
    (void)state;
    cursor->pos.c = to_char;
    if (cursor->pos.c < 0)
    {
        if (cursor->pos.l > 0 && can_switch_line)
        {
            cursor->pos.l--;
            cursor->pos.c = text_buffer->lines[cursor->pos.l].len - 1;
        }
        else
        {
            cursor->pos.c = 0;
        }
    }
    if (cursor->pos.c >= text_buffer->lines[cursor->pos.l].len)
    {
        if (cursor->pos.l < text_buffer->line_count - 1 && can_switch_line)
        {
            cursor->pos.l++;
            cursor->pos.c = 0;
        }
        else
        {
            cursor->pos.c = text_buffer->lines[cursor->pos.l].len - 1;
        }
    }
    if (cursor->pos.c != prev_cursor_char)
    {
        cursor->frame_count = 0;
        if (snap_viewport)
        {
            viewport_snap_to_cursor(state);
        }
    }
}

void move_cursor_to_next_end_of_word(Editor_State *state)
{
    Text_Line *current_line = &state->text_buffer.lines[state->cursor.pos.l];
    int end_of_word = -1;
    for (int i = state->cursor.pos.c + 1; i < current_line->len - 1; i++)
    {
        if (isalnum(current_line->str[i]) && (isspace(current_line->str[i + 1]) || ispunct(current_line->str[i + 1])))
        {
            end_of_word = i + 1;
            break;
        }
    }
    if (end_of_word == -1)
    {
        move_cursor_to_line(&state->text_buffer, &state->cursor, state, state->cursor.pos.l + 1, true);
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, true, false);
    }
    else
    {
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, end_of_word, true, false);
    }
}

void move_cursor_to_prev_start_of_word(Editor_State *state)
{
    Text_Line *current_line = &state->text_buffer.lines[state->cursor.pos.l];
    int start_of_word = -1;
    for (int i = state->cursor.pos.c - 1; i > 0; i--)
    {
        if (isalnum(current_line->str[i]) && (isspace(current_line->str[i - 1]) || ispunct(current_line->str[i - 1])))
        {
            start_of_word = i;
            break;
        }
    }
    if (start_of_word == -1)
    {
        move_cursor_to_line(&state->text_buffer, &state->cursor, state, state->cursor.pos.l - 1, true);
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, MAX_CHARS_PER_LINE, true, false);
    }
    else
    {
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, start_of_word, true, false);
    }
}

void move_cursor_to_next_white_line(Editor_State *state)
{
    for (int i = state->cursor.pos.l + 1; i < state->text_buffer.line_count; i++)
    {
        if (is_white_line(state->text_buffer.lines[i]))
        {
            move_cursor_to_line(&state->text_buffer, &state->cursor, state, i, true);
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, true, false);
            return;
        }
    }
    move_cursor_to_line(&state->text_buffer, &state->cursor, state, MAX_LINES, true);
    move_cursor_to_char(&state->text_buffer, &state->cursor, state, MAX_CHARS_PER_LINE, true, false);
}

void move_cursor_to_prev_white_line(Editor_State *state)
{
    for (int i = state->cursor.pos.l - 1; i >= 0; i--)
    {
        if (is_white_line(state->text_buffer.lines[i]))
        {
            move_cursor_to_line(&state->text_buffer, &state->cursor, state, i, true);
            move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, true, false);
            return;
        }
    }
    move_cursor_to_line(&state->text_buffer, &state->cursor, state, 0, true);
    move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, true, false);
}

Text_Line make_text_line_dup(char *line)
{
    Text_Line r;
    r.str = xstrdup(line);
    r.len = strlen(r.str);
    r.buf_len = r.len + 1;
    return r;
}

Text_Line copy_text_line(Text_Line source, int start, int end)
{
    Text_Line r;
    if (end < 0) end = source.len;
    r.len = end - start;
    r.buf_len = r.len + 1;
    r.str = xmalloc(r.buf_len);
    strncpy(r.str, source.str + start, r.len);
    r.str[r.len] = '\0';
    return r;
}

void resize_text_line(Text_Line *text_line, int new_size) {
    bassert(new_size <= MAX_CHARS_PER_LINE);
    text_line->str = xrealloc(text_line->str, new_size + 1);
    text_line->buf_len = new_size + 1;
    text_line->len = new_size;
    text_line->str[new_size] = '\0';
}

void insert_line(Text_Buffer *text_buffer, Text_Line new_line, int insert_at)
{
    text_buffer->lines = xrealloc(text_buffer->lines, (text_buffer->line_count + 1) * sizeof(text_buffer->lines[0]));
    for (int i = text_buffer->line_count - 1; i >= insert_at ; i--) {
        text_buffer->lines[i + 1] = text_buffer->lines[i];
    }
    text_buffer->line_count++;
    text_buffer->lines[insert_at] = new_line;
}

void remove_line(Text_Buffer *text_buffer, int remove_at)
{
    free(text_buffer->lines[remove_at].str);
    for (int i = remove_at + 1; i <= text_buffer->line_count - 1; i++) {
        text_buffer->lines[i - 1] = text_buffer->lines[i];
    }
    text_buffer->line_count--;
    text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
}

void insert_char(Text_Buffer *text_buffer, char c, Text_Cursor *cursor, Editor_State *state, bool auto_indent)
{
    if (c == '\n') {
        Text_Line new_line = make_text_line_dup(text_buffer->lines[cursor->pos.l].str + cursor->pos.c);
        insert_line(text_buffer, new_line, cursor->pos.l + 1);
        resize_text_line(&text_buffer->lines[cursor->pos.l], cursor->pos.c + 1);
        text_buffer->lines[cursor->pos.l].str[cursor->pos.c] = '\n';
        move_cursor_to_line(text_buffer, cursor, state, cursor->pos.l + 1, true);
        move_cursor_to_char(text_buffer, cursor, state, 0, true, false);
        if (auto_indent)
        {
            int indent_spaces = get_line_indent(text_buffer->lines[cursor->pos.l - 1]);
            for (int i = 0; i < indent_spaces; i++)
            {
                insert_char(&state->text_buffer, ' ', &state->cursor, state, false);
            }
        }

    } else {
        resize_text_line(&text_buffer->lines[cursor->pos.l], text_buffer->lines[cursor->pos.l].len + 1);
        Text_Line *line = &text_buffer->lines[cursor->pos.l];
        for (int i = line->len - 1; i >= cursor->pos.c; i--) {
            line->str[i + 1] = line->str[i];
        }
        line->str[cursor->pos.c] = c;
        move_cursor_to_char(text_buffer, cursor, state, cursor->pos.c + 1, true, false);
    }
}

void remove_char(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state)
{
    if (cursor->pos.c <= 0) {
        if (cursor->pos.l <= 0) {
            return;
        }
        Text_Line *line0 = &text_buffer->lines[cursor->pos.l - 1];
        Text_Line *line1 = &text_buffer->lines[cursor->pos.l];
        int line0_old_len = line0->len;
        resize_text_line(line0, line0_old_len - 1 + line1->len);
        strcpy(line0->str + line0_old_len - 1, line1->str);
        remove_line(text_buffer, cursor->pos.l);
        move_cursor_to_line(text_buffer, cursor, state, cursor->pos.l - 1, true);
        move_cursor_to_char(text_buffer, cursor, state, line0_old_len - 1, true, false);
    } else {
        Text_Line *line = &text_buffer->lines[cursor->pos.l];
        for (int i = cursor->pos.c; i <= line->len; i++) {
            line->str[i - 1] = line->str[i];
        }
        resize_text_line(&text_buffer->lines[cursor->pos.l], line->len - 1);
        move_cursor_to_char(text_buffer, cursor, state, cursor->pos.c - 1, true, false);
    }
}

void insert_indent(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state)
{
    if (is_selection_valid(state))
    {
        delete_selected(state);
    }
    int spaces_to_insert = INDENT_SPACES - cursor->pos.c % INDENT_SPACES;
    for (int i = 0; i < spaces_to_insert; i++)
    {
        insert_char(text_buffer, ' ', cursor, state, false);
    }
}

int get_line_indent(Text_Line line)
{
    int spaces = 0;
    char *str = line.str;
    while (*str)
    {
        if (*str == ' ') spaces++;
        else return spaces;
        str++;
    }
    return 0;
}

void decrease_indent_level_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state)
{
    int indent = get_line_indent(text_buffer->lines[cursor->pos.l]);
    int chars_to_remove = indent % INDENT_SPACES;
    if (indent >= INDENT_SPACES && chars_to_remove == 0)
    {
        chars_to_remove = 4;
    }
    move_cursor_to_char(text_buffer, cursor, state, indent, false, false);
    for (int i = 0; i < chars_to_remove; i++)
    {
        remove_char(text_buffer, cursor, state);
    }
}

void decrease_indent_level(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state)
{
    if (is_selection_valid(state))
    {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l)
        {
            Buf_Pos temp = start;
            start = end;
            end = temp;
        }
        for (int i = start.l; i <= end.l; i++)
        {
            move_cursor_to_line(text_buffer, cursor, state, i, false);
            decrease_indent_level_line(text_buffer, cursor, state);
        }
        move_cursor_to_line(text_buffer, cursor, state, start.l, false);
        int indent = get_line_indent(text_buffer->lines[cursor->pos.l]);
        move_cursor_to_char(text_buffer, cursor, state, indent, false, false);
    }
    else
    {
        decrease_indent_level_line(text_buffer, cursor, state);
    }
}

void increase_indent_level_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state)
{
    int indent = get_line_indent(text_buffer->lines[cursor->pos.l]);
    int chars_to_add = INDENT_SPACES - (indent % INDENT_SPACES);
    move_cursor_to_char(text_buffer, cursor, state, indent, false, false);
    for (int i = 0; i < chars_to_add; i++)
    {
        insert_char(text_buffer, ' ', cursor, state, false);
    }
}

void increase_indent_level(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state)
{
    if (is_selection_valid(state))
    {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l)
        {
            Buf_Pos temp = start;
            start = end;
            end = temp;
        }
        for (int i = start.l; i <= end.l; i++)
        {
            move_cursor_to_line(text_buffer, cursor, state, i, false);
            increase_indent_level_line(text_buffer, cursor, state);
        }
        move_cursor_to_line(text_buffer, cursor, state, start.l, false);
        int indent = get_line_indent(text_buffer->lines[cursor->pos.l]);
        move_cursor_to_char(text_buffer, cursor, state, indent, false, false);
    }
    else
    {
        increase_indent_level_line(text_buffer, cursor, state);
    }
}

void delete_current_line(Editor_State *state)
{
    remove_line(&state->text_buffer, state->cursor.pos.l);
    move_cursor_to_line(&state->text_buffer, &state->cursor, state, state->cursor.pos.l, false);
    move_cursor_to_char(&state->text_buffer, &state->cursor, state, 0, false, false);
}

bool is_white_line(Text_Line line)
{
    for (int i = 0; i < line.len; i++)
    {
        if (!isspace(line.str[i]))
        {
            return false;
        }
    }
    return true;
}

void open_file_for_edit(const char *path, Editor_State *state)
{
    state->file_info = read_file(path, &state->text_buffer);
    trace_log("Read file at %s", path);
    move_cursor_to_line(&state->text_buffer, &state->cursor, state, state->cursor.pos.l, false);
    move_cursor_to_char(&state->text_buffer, &state->cursor, state, state->cursor.pos.c, false, false);
}

File_Info read_file(const char *path, Text_Buffer *text_buffer)
{
    File_Info file_info = {0};
    file_info.path = xstrdup(path);
    FILE *f = fopen(file_info.path, "r");
    if (!f) fatal("Failed to open file for reading at %s", path);
    char buf[MAX_CHARS_PER_LINE];
    memset(text_buffer, 0, sizeof(*text_buffer));
    while (fgets(buf, sizeof(buf), f)) {
        text_buffer->line_count++;
        bassert(text_buffer->line_count <= MAX_LINES);
        text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
        text_buffer->lines[text_buffer->line_count - 1] = make_text_line_dup(buf);
    }
    fclose(f);
    return file_info;
}

void write_file(Text_Buffer text_buffer, File_Info file_info)
{
    FILE *f = fopen(file_info.path, "w");
    if (!f) fatal("Failed to open file for writing at %s", file_info.path);
    for (int i = 0; i < text_buffer.line_count; i++) {
        fputs(text_buffer.lines[i].str, f);
    }
    fclose(f);
    trace_log("Saved file to %s", file_info.path);
}

void validate_text_buffer(Text_Buffer *text_buffer)
{
    for (int i = 0; i < text_buffer->line_count; i++) {
        int actual_len = strlen(text_buffer->lines[i].str);
        bassert(actual_len > 0);
        bassert(actual_len == text_buffer->lines[i].len);
        bassert(text_buffer->lines[i].buf_len == text_buffer->lines[i].len + 1);
        bassert(text_buffer->lines[i].str[actual_len] == '\0');
        bassert(text_buffer->lines[i].str[actual_len - 1] == '\n');
    }
}

void start_selection_at_cursor(Editor_State *state)
{
    state->selection.start = state->cursor.pos;
    state->selection.end = state->selection.start;
}

void extend_selection_to_cursor(Editor_State *state)
{
    state->selection.end = state->cursor.pos;
}

bool is_selection_valid(const Editor_State *state)
{
    const Text_Selection *sel = &state->selection;
    bool is_valid = is_buf_pos_valid(&state->text_buffer, sel->start) &&
        is_buf_pos_valid(&state->text_buffer, sel->end) &&
        !is_buf_pos_equal(sel->start, sel->end);
    return is_valid;
}

void cancel_selection(Editor_State *state)
{
    state->selection.start = (Buf_Pos){ -1, -1 };
    state->selection.end = (Buf_Pos){ -1, -1 };
}

int selection_char_count(Editor_State *state)
{
    int char_count = 0;
    if (is_selection_valid(state))
    {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l)
        {
            Buf_Pos temp = start;
            start = end;
            end = temp;
        }
        for (int i = start.l; i <= end.l; i++)
        {
            if (i == start.l && i == end.l)
            {
                int diff = end.c - start.c;
                if (diff < 0) diff = -diff;
                char_count += diff;
            }
            else if (i == start.l)
            {
                char_count += state->text_buffer.lines[i].len - start.c;
            }
            else if (i == end.l)
            {
                char_count += end.c;
            }
            else
            {
                char_count += state->text_buffer.lines[i].len;
            }
        }
    }
    return char_count;
}

void delete_selected(Editor_State *state)
{
    int char_count = selection_char_count(state);
    if (char_count > 0)
    {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l || (start.l == end.l && start.c > end.c))
        {
            Buf_Pos temp = start;
            start = end;
            end = temp;
        }
        move_cursor_to_line(&state->text_buffer, &state->cursor, state, end.l, false);
        move_cursor_to_char(&state->text_buffer, &state->cursor, state, end.c, false, false);
        for (int i = 0; i < char_count; i++)
        {
            remove_char(&state->text_buffer, &state->cursor, state);
        }
        cancel_selection(state);
    }
}

void copy_at_selection(Editor_State *state)
{
    if (is_selection_valid(state))
    {
        Buf_Pos start = state->selection.start;
        Buf_Pos end = state->selection.end;
        if (start.l > end.l)
        {
            Buf_Pos temp = start;
            start = end;
            end = temp;
        }
        state->copy_buffer.line_count = end.l - start.l + 1;
        state->copy_buffer.lines = xrealloc(state->copy_buffer.lines, state->copy_buffer.line_count * sizeof(state->copy_buffer.lines[0]));
        int copy_buffer_i = 0;
        for (int i = start.l; i <= end.l; i++)
        {
            int start_c, end_c;
            if (i == start.l && i == end.l)
            {
                start_c = start.c;
                end_c = end.c;
                if (start_c > end_c)
                {
                    int temp = start_c;
                    start_c = end_c;
                    end_c = temp;
                }
            }
            else if (i == start.l)
            {
                start_c = start.c;
                end_c = -1;
            }
            else if (i == end.l)
            {
                start_c = 0;
                end_c = end.c;
            }
            else
            {
                start_c = 0;
                end_c = -1;
            }
            state->copy_buffer.lines[copy_buffer_i++] = copy_text_line(state->text_buffer.lines[i], start_c, end_c);
        }
        trace_log("Copied at selection");
    }
    else
    {
        trace_log("Nothing to copy");
    }
}

void paste_from_copy_buffer(Editor_State *state)
{
    if (state->copy_buffer.line_count > 0)
    {
        if (is_selection_valid(state))
        {
            delete_selected(state);
        }
        for (int i = 0; i < state->copy_buffer.line_count; i++)
        {
            for (int char_i = 0; char_i < state->copy_buffer.lines[i].len; char_i++)
            {
                insert_char(&state->text_buffer, state->copy_buffer.lines[i].str[char_i], &state->cursor, state, false);
            }
        }
    }
    else
    {
        trace_log("Nothing to paste");
    }
}

void rebuild_dl()
{
    int result = system("make dl");
    if (result != 0) {
        fprintf(stderr, "Build failed with code %d\n", result);
    }
    trace_log("Rebuilt dl");
}

void insert_go_to_line_char(Editor_State *state, char c)
{
    bassert(state->current_go_to_line_char_i < GO_TO_LINE_CHAR_MAX);
    state->go_to_line_chars[state->current_go_to_line_char_i++] = c;
}

void convert_to_debug_invis(char *string)
{
    while (*string) {
        if (*string == ' ') {
            *string = '.';
        } else if (*string == '\n') {
            *string = '-';
        }
        string++;
    }
}
