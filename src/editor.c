#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_easy_font.h>

#define VERT_MAX 4096
#define CURSOR_BLINK_ON_FRAMES 30
#define SCROLL_SENS 10.0f
// #define FILE_PATH "src/editor.c"
#define FILE_PATH "res/mock3.txt"

const char *vs_src =
"#version 410 core\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec4 aColor;\n"
"uniform mat4 u_mvp;\n"
"out vec4 Color;\n"
"void main() {\n"
"    gl_Position = u_mvp * vec4(aPos, 0.0, 1.0);\n"
"    Color = aColor;\n"
"}";

const char *fs_src =
"#version 410 core\n"
"out vec4 FragColor;\n"
"in vec4 Color;\n"
"void main() {\n"
"    FragColor = Color;\n"
"}";

typedef struct {
    float x, y;
} Vec_2;
#define VEC2_FMT "<%0.2f, %0.2f>"
#define VEC2_ARG(v) (v).x, (v).y

typedef struct {
    Vec_2 min;
    Vec_2 max;
} Rect_Bounds;

typedef struct {
    float x, y;
    unsigned char r, g, b, a;
} Vert;

typedef struct {
    Vert verts[VERT_MAX];
    int vert_count;
} Vert_Buffer;

typedef struct {
    Vec_2 offset;
    float zoom;
} Viewport;

typedef struct {
    char *path;
} File_Info;

typedef struct {
    char **lines;
    int line_count;
} Text_Buffer;

typedef struct {
    int line_i;
    int char_i;
} Cursor;

typedef struct {
    GLuint prog;
    GLuint vao;
    GLuint vbo;
    GLuint shader_mvp_loc;
    float view_transform[16];
    float proj_transform[16];
    Viewport viewport;
    Vec_2 window_dim;
    File_Info file_info;
    Text_Buffer text_buffer;
    Cursor cursor;
    bool should_break;
    bool debug_invis;
    int cursor_frame_count;
    long long frame_count;
} Editor_State;

void char_callback(GLFWwindow *window, unsigned int codepoint);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
void framebuffer_size_callback(GLFWwindow *window, int w, int h);
void window_size_callback(GLFWwindow *window, int w, int h);
void refresh_callback(GLFWwindow *window);

void render_string(int x, int y, char *line, unsigned char color[4], Vert_Buffer *out_vert_buf);
void draw_quad(int x, int y, int width, int height, unsigned char color[4]);
void draw_string(int x, int y, char *string, unsigned char color[4]);
void draw_content_string(int x, int y, char *string, unsigned char color[4], Editor_State *state);

void insert_char(Text_Buffer *text_buffer, char c, Cursor *cursor, Editor_State *state);
void remove_char(Text_Buffer *text_buffer, Cursor *cursor, Editor_State *state);

void convert_to_debug_invis(char *string);
void move_cursor(Text_Buffer *text_buffer, Cursor *cursor, int line_i, int char_i, bool char_switch_line, Editor_State *state);

void insert_line(Text_Buffer *text_buffer, char *line, int insert_at);
void remove_line(Text_Buffer *text_buffer, int remove_at);

File_Info read_file(const char *path, Text_Buffer *text_buffer);
void write_file(Text_Buffer text_buffer, File_Info file_info);

void rebuild_dl();

void make_ortho(float left, float right, float bottom, float top, float near, float far, float *out);
void make_view(float offset_x, float offset_y, float scale, float *out);
void mul_mat4(const float *a, const float *b, float *out);

Rect_Bounds get_viewport_bounds(Vec_2 window_dim, Viewport viewport);
Vec_2 get_viewport_dim(Vec_2 window_dim, Viewport viewport);
Vec_2 window_pos_to_canvas_pos(Vec_2 window_pos, Viewport viewport);
void update_shader_mvp(Editor_State *state);

void _init(GLFWwindow *window, void *_state)
{
    (void)window; Editor_State *state = _state; (void)state;

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
        GLuint shader = vs;
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), NULL, log);
            fprintf(stderr, "Shader compile error:\n%s\nSource:\n", log);
            fprintf(stderr, "%s\n", vs_src);
        }
    }
    state->prog = glCreateProgram();
    glAttachShader(state->prog, vs);
    glAttachShader(state->prog, fs);
    glLinkProgram(state->prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    glUseProgram(state->prog);

    glGenVertexArrays(1, &state->vao);
    glGenBuffers(1, &state->vbo);
    glBindVertexArray(state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
    glBufferData(GL_ARRAY_BUFFER,  VERT_MAX * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (void *)offsetof(Vert, r));
    glEnableVertexAttribArray(1);
    glfwSetWindowUserPointer(window, _state);

    state->shader_mvp_loc = glGetUniformLocation(state->prog, "u_mvp");
    state->viewport.zoom = 1.0f;
    int window_w, window_h;
    glfwGetWindowSize(window, &window_w, &window_h);
    state->window_dim.x = window_w;
    state->window_dim.y = window_h;
    update_shader_mvp(state);

    state->file_info = read_file(FILE_PATH, &state->text_buffer);
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
}

void _render(GLFWwindow *window, void *_state)
{
    (void)window; Editor_State *state = _state; (void)state;

    if (state->should_break) {
        __builtin_debugtrap();
        state->should_break = false;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    int x_canvas_offset = 0;
    int y_canvas_offset = 0;
    int line_height = 15;
    int x_line_num_offset = stb_easy_font_width("000: ");

    int lines_rendered = 0;

    int x_offset = x_canvas_offset;
    int y_offset = y_canvas_offset;
    for (int line_i = 0; line_i < state->text_buffer.line_count; line_i++) {
        // if ((line_i + 1) * line_height > viewport_y_min && line_i * line_height < viewport_y_max) {
        {
            unsigned char color[] = { 240, 240, 240, 240 };
            if (line_i == state->cursor.line_i) {
                color[0] = 50;
                color[1] = 50;
                color[2] = 255;
                color[0] = 255;
            }

            char line_i_str_buf[256];
            snprintf(line_i_str_buf, sizeof(line_i_str_buf), "%3d: ", line_i + 1);
            draw_string(x_offset, y_offset, line_i_str_buf, color);
            x_offset += x_line_num_offset;

            draw_content_string(x_offset, y_offset, state->text_buffer.lines[line_i], color, state);

            x_offset = x_canvas_offset;
            lines_rendered++;
        }

        y_offset += line_height;
    }

    if (!((state->cursor_frame_count % (CURSOR_BLINK_ON_FRAMES * 2)) / CURSOR_BLINK_ON_FRAMES))
    {
        int cursor_x_offset = stb_easy_font_width_up_to(state->text_buffer.lines[state->cursor.line_i], state->cursor.char_i);
        int cursor_x = x_canvas_offset + x_line_num_offset + cursor_x_offset;
        int cursor_y = y_canvas_offset + line_height * state->cursor.line_i;
        int cursor_width = stb_easy_font_char_width(state->text_buffer.lines[state->cursor.line_i][state->cursor.char_i]);
        if (cursor_width == 0) cursor_width = 6;
        int cursor_height = 12;
        unsigned char color[] = { 200, 200, 200, 255 };
        draw_quad(cursor_x, cursor_y, cursor_width, cursor_height, color);
    }

    {
        Vec_2 viewport_dim = get_viewport_dim(state->window_dim, state->viewport);
        unsigned char bg_color[] = { 30, 30, 30, 255 };
        draw_quad(state->viewport.offset.x, state->viewport.offset.y + viewport_dim.y - 2 * line_height - 3, viewport_dim.x, 3 + 2 * line_height, bg_color);

        char line_i_str_buf[256];
        snprintf(line_i_str_buf, sizeof(line_i_str_buf),
            "STATUS: Cursor: %d, %d; Line Len: %zu; Invis: %d; Lines: %d; Rendered: %d",
            state->cursor.line_i,
            state->cursor.char_i,
            strlen(state->text_buffer.lines[state->cursor.line_i]),
            state->debug_invis,
            state->text_buffer.line_count,
            lines_rendered);
        unsigned char color[] = { 200, 200, 200, 255 };
        draw_string(state->viewport.offset.x + 10, state->viewport.offset.y + viewport_dim.y - 2 * line_height, line_i_str_buf, color);

        Rect_Bounds viewport_bounds = get_viewport_bounds(state->window_dim, state->viewport);
        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        Vec_2 mouse_window_pos = { .x = mouse_x, .y = mouse_y };
        Vec_2 mouse_canvas_pos = window_pos_to_canvas_pos(mouse_window_pos, state->viewport);
        (void)mouse_canvas_pos;

        snprintf(line_i_str_buf, sizeof(line_i_str_buf),
            "Viewport bounds: " VEC2_FMT VEC2_FMT "; Zoom: %0.2f; Mouse Position: window: " VEC2_FMT " canvas: " VEC2_FMT,
            VEC2_ARG(viewport_bounds.min),
            VEC2_ARG(viewport_bounds.max),
            state->viewport.zoom,
            VEC2_ARG(mouse_window_pos),
            VEC2_ARG(mouse_canvas_pos));
        printf("%d chars\n", (int)strlen(line_i_str_buf));
        draw_string(state->viewport.offset.x + 10, state->viewport.offset.y + viewport_dim.y - line_height, line_i_str_buf, color);
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    state->frame_count++;
    state->cursor_frame_count++;
}

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window);
    if (codepoint < 128) {
        insert_char(&state->text_buffer, (char)codepoint, &state->cursor, state);
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods; Editor_State *state = glfwGetWindowUserPointer(window);
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
    } else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        if (mods == GLFW_MOD_SUPER) {
            move_cursor(&state->text_buffer, &state->cursor, 10000, 10000, false, state);
        } else {
            move_cursor(&state->text_buffer, &state->cursor, state->cursor.line_i + 1, state->cursor.char_i, false, state);
        }
    } else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        if (mods == GLFW_MOD_SUPER) {
            move_cursor(&state->text_buffer, &state->cursor, 0, 0, false, state);
        } else {
            move_cursor(&state->text_buffer, &state->cursor, state->cursor.line_i - 1, state->cursor.char_i, false, state);
        }
    } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        if (mods == GLFW_MOD_SUPER) {
            move_cursor(&state->text_buffer, &state->cursor, state->cursor.line_i, 0, false, state);
        } else {
            move_cursor(&state->text_buffer, &state->cursor, state->cursor.line_i, state->cursor.char_i - 1, true, state);
        }
    } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        if (mods == GLFW_MOD_SUPER) {
            move_cursor(&state->text_buffer, &state->cursor, state->cursor.line_i, 10000, false, state);
        } else {
            move_cursor(&state->text_buffer, &state->cursor, state->cursor.line_i, state->cursor.char_i + 1, true, state);
        }
    } else if (key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        insert_char(&state->text_buffer, '\n', &state->cursor, state);
    } else if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        remove_char(&state->text_buffer, &state->cursor, state);
    } else if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        state->debug_invis = !state->debug_invis;
    } else if (key == GLFW_KEY_F12 && action == GLFW_PRESS) {
        state->should_break = true;
    } else if (key ==GLFW_KEY_F9 && action == GLFW_PRESS) {
        rebuild_dl();
    } else if (key == GLFW_KEY_S && mods == GLFW_MOD_SUPER && action == GLFW_PRESS) {
        write_file(state->text_buffer, state->file_info);
    } else if (key == GLFW_KEY_EQUAL && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        state->viewport.zoom += 0.25f;
        update_shader_mvp(state);
    } else if (key == GLFW_KEY_MINUS && mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        state->viewport.zoom -= 0.25f;
        update_shader_mvp(state);
    }
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
    state->window_dim.x = w;
    state->window_dim.y = h;
    update_shader_mvp(state);
}

void refresh_callback(GLFWwindow* window) {
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    _render(window, state);
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

void draw_quad(int x, int y, int width, int height, unsigned char color[4])
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

void draw_string(int x, int y, char *string, unsigned char color[4])
{
    Vert_Buffer vert_buf = {0};
    render_string(x, y, string, color, &vert_buf);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_content_string(int x, int y, char *string, unsigned char color[4], Editor_State *state)
{
    if (state->debug_invis) {
        string = strdup(string);
        convert_to_debug_invis(string);
    }
    draw_string(x, y, string, color);
    if (state->debug_invis) {
        free(string);
    }
}

void insert_char(Text_Buffer *text_buffer, char c, Cursor *cursor, Editor_State *state)
{
    if (c == '\n') {
        char *next_line = strdup(text_buffer->lines[cursor->line_i] + cursor->char_i);
        insert_line(text_buffer, next_line, cursor->line_i + 1);
        text_buffer->lines[cursor->line_i] = realloc(text_buffer->lines[cursor->line_i], cursor->char_i + 2);
        text_buffer->lines[cursor->line_i][cursor->char_i] = '\n';
        text_buffer->lines[cursor->line_i][cursor->char_i + 1] = '\0';
        move_cursor(text_buffer, cursor, cursor->line_i + 1, 0, false, state);
    } else {
        size_t len = strlen(text_buffer->lines[cursor->line_i]);
        text_buffer->lines[cursor->line_i] = realloc(text_buffer->lines[cursor->line_i], len + 2);
        char *line = text_buffer->lines[cursor->line_i];
        for (int i = len; i >= cursor->char_i; i--) {
            line[i + 1] = line[i];
        }
        line[cursor->char_i] = c;
        move_cursor(text_buffer, cursor, cursor->line_i, cursor->char_i + 1, false, state);
    }
}

void remove_char(Text_Buffer *text_buffer, Cursor *cursor, Editor_State *state)
{
    if (cursor->char_i <= 0) {
        if (cursor->line_i <= 0) {
            return;
        }
        char *line1 = text_buffer->lines[cursor->line_i];
        int len1 = strlen(line1);
        char *line0 = text_buffer->lines[cursor->line_i - 1];
        int len0 = strlen(line0);
        line0 = realloc(line0, len0 + len1);
        strcpy(line0 + len0 - 1, line1);
        text_buffer->lines[cursor->line_i - 1] = line0;
        remove_line(text_buffer, cursor->line_i);
        move_cursor(text_buffer, cursor, cursor->line_i - 1, len0 - 1, false, state);
    } else {
        char *line = text_buffer->lines[cursor->line_i];
        size_t len = strlen(line);
        for (int i = cursor->char_i; i <= (int)len; i++) {
            line[i - 1] = line[i];
        }
        text_buffer->lines[cursor->line_i] = realloc(line, len);
        move_cursor(text_buffer, cursor, cursor->line_i, cursor->char_i - 1, false, state);
    }
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

// TODO Probably refactor into multiple functions
void move_cursor(Text_Buffer *text_buffer, Cursor *cursor, int line_i, int char_i, bool char_switch_line, Editor_State *state)
{
    int orig_cursor_line = cursor->line_i;
    int prev_cursor_char = cursor->char_i;
    if (orig_cursor_line != line_i) {
        cursor->line_i = line_i;
        if (cursor->line_i < 0) {
            cursor->line_i = 0;
            cursor->char_i = 0;
        }
        if (cursor->line_i >= text_buffer->line_count) {
            cursor->line_i = text_buffer->line_count - 1;
            cursor->char_i = strlen(text_buffer->lines[cursor->line_i]) - 1;
        }
    }
    if (prev_cursor_char != char_i) {
        cursor->char_i = char_i;
        if (cursor->char_i < 0) {
            if (cursor->line_i > 0 && char_switch_line) {
                cursor->line_i--;
                int line_len = strlen(text_buffer->lines[cursor->line_i]);
                cursor->char_i = line_len - 1;
            } else {
                cursor->char_i = 0;
            }
        }
        int line_len = strlen(text_buffer->lines[cursor->line_i]);
        if (cursor->char_i >= line_len) {
            if (cursor->line_i < text_buffer->line_count - 1 && char_switch_line) {
                cursor->line_i++;
                cursor->char_i = 0;
            } else {
                cursor->char_i = line_len - 1;
            }
        }
    }
    if (cursor->line_i != orig_cursor_line || cursor->char_i != prev_cursor_char) {
        state->cursor_frame_count = 0;
    }
}

void insert_line(Text_Buffer *text_buffer, char *line, int insert_at)
{
    text_buffer->lines = realloc(text_buffer->lines, (text_buffer->line_count + 1) * sizeof(char *));
    for (int i = text_buffer->line_count - 1; i >= insert_at ; i--) {
        text_buffer->lines[i + 1] = text_buffer->lines[i];
    }
    text_buffer->line_count++;
    text_buffer->lines[insert_at] = line;
}

void remove_line(Text_Buffer *text_buffer, int remove_at)
{
    free(text_buffer->lines[remove_at]);
    for (int i = remove_at + 1; i <= text_buffer->line_count - 1; i++) {
        text_buffer->lines[i - 1] = text_buffer->lines[i];
    }
    text_buffer->line_count--;
    text_buffer->lines = realloc(text_buffer->lines, text_buffer->line_count * sizeof(char *));
}

File_Info read_file(const char *path, Text_Buffer *text_buffer)
{
    File_Info file_info = {0};
    file_info.path = strdup(path);
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen for read");
        exit(1);
    }
    int count = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        text_buffer->lines = realloc(text_buffer->lines, (count + 1) * sizeof(char *));
        text_buffer->lines[count++] = strdup(buf);
    }
    text_buffer->line_count = count;
    fclose(f);
    return file_info;
}

void write_file(Text_Buffer text_buffer, File_Info file_info)
{
    FILE *f = fopen(file_info.path, "w");
    if (!f) {
        perror("fopen for write");
        exit(1);
    }

    for (int i = 0; i < text_buffer.line_count; i++) {
        fputs(text_buffer.lines[i], f);
    }

    fclose(f);

    printf("Saved file to %s\n", file_info.path);
}

void rebuild_dl()
{
    int result = system("make dl");
    if (result != 0) {
        fprintf(stderr, "Build failed with code %d\n", result);
    }
    printf("Rebuilt dl\n");
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

Rect_Bounds get_viewport_bounds(Vec_2 window_dim, Viewport viewport)
{
    Rect_Bounds r = {0};
    r.min.x = viewport.offset.x;
    r.min.y = viewport.offset.y;
    r.max.x = r.min.x + window_dim.x / viewport.zoom;
    r.max.y = r.min.y + window_dim.y / viewport.zoom;
    return r;
}

Vec_2 get_viewport_dim(Vec_2 window_dim, Viewport viewport)
{
    Vec_2 r = {0};
    r.x = window_dim.x / viewport.zoom;
    r.y = window_dim.y / viewport.zoom;
    return r;
}

Vec_2 window_pos_to_canvas_pos(Vec_2 window_pos, Viewport viewport)
{
    Vec_2 r = {0};
    r.x = viewport.offset.x + window_pos.x / viewport.zoom;
    r.y = viewport.offset.y + window_pos.y / viewport.zoom;
    return r;
}

void update_shader_mvp(Editor_State *state)
{
    make_ortho(0, state->window_dim.x, state->window_dim.y, 0, -1, 1, state->proj_transform);
    make_view(state->viewport.offset.x, state->viewport.offset.y, state->viewport.zoom, state->view_transform);
    float mvp[16];
    mul_mat4(state->view_transform, state->proj_transform, mvp);
    glUniformMatrix4fv(state->shader_mvp_loc, 1, GL_FALSE, mvp);
}
