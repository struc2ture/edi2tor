#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_easy_font.h>

#define VERT_MAX 4096

const char *vs_src =
"#version 410 core\n"
"layout (location = 0) in vec2 aPos;\n"
"layout (location = 1) in vec4 aColor;\n"
"out vec4 Color;\n"
"void main() {\n"
"    vec2 zeroToOne = aPos / vec2(800, 600);\n"
"    vec2 zeroToTwo = zeroToOne * 2.0;\n"
"    vec2 clipSpace = zeroToTwo - 1.0;\n"
"    gl_Position = vec4(clipSpace.x, -clipSpace.y, 0.0, 1.0);\n"
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
    char **lines;
    int line_count;
} Text_Buffer;

typedef struct {
    int line_i;
    int char_i;
} Cursor;

typedef struct {
    float x, y;
    unsigned char r, g, b, a;
} Vert;

typedef struct {
    Vert verts[VERT_MAX];
    int vert_count;
} Vert_Buffer;

static bool g_debug_invis = false;

static Text_Buffer g_text_buffer = {0};
static Cursor g_cursor = { .line_i = 1, .char_i = 0};

static int g_cursor_blink_on_frames = 30;
static int g_cursor_frame_count = 0;

void char_callback(GLFWwindow *window, unsigned int codepoint);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

Text_Buffer read_file(const char *path);
void render_string(int x, int y, char *line, unsigned char color[4], Vert_Buffer *out_vert_buf);
void draw_quad(int x, int y, int width, int height, unsigned char color[4]);
void draw_string(int x, int y, char *string, unsigned char color[4]);
void draw_content_string(int x, int y, char *string, unsigned char color[4]);

void insert_char(Text_Buffer *text_buffer, char c, Cursor *cursor);
void remove_char(Text_Buffer *text_buffer, Cursor *cursor);

void convert_to_debug_invis(char *string);
void move_cursor(Text_Buffer *text_buffer, Cursor *cursor, int line_i, int char_i);

void insert_line(Text_Buffer *text_buffer, char *line, int insert_at);
void remove_line(Text_Buffer *text_buffer, int remove_at);

int main()
{
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "EDITOR", NULL, NULL);
    if (!window) return -1;

    glfwMakeContextCurrent(window);

    glfwSetKeyCallback(window, key_callback);
    glfwSetCharCallback(window, char_callback);

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, 0);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, 0);
    glCompileShader(fs);
    {
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
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,  VERT_MAX * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (void *)offsetof(Vert, r));
    glEnableVertexAttribArray(1);

    g_text_buffer = read_file("res/mock3.txt");

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);

        int x_padding = 10;
        int y_padding = 10;
        int line_height = 20;
        int x_line_num_offset = stb_easy_font_width("000: ");

        int x_offset = x_padding;
        int y_offset = y_padding;

        for (int line_i = 0; line_i < g_text_buffer.line_count; line_i++) {
            unsigned char color[] = { 240, 240, 240, 240 };
            if (line_i == g_cursor.line_i) {
                color[0] = 50;
                color[1] = 50;
                color[2] = 255;
                color[0] = 255;
            }

            char line_i_str_buf[256];
            snprintf(line_i_str_buf, sizeof(line_i_str_buf), "%3d: ", line_i);

            draw_string(x_offset, y_offset, line_i_str_buf, color);

            x_offset += x_line_num_offset;

            draw_content_string(x_offset, y_offset, g_text_buffer.lines[line_i], color);

            x_offset = x_padding;
            y_offset += line_height;
        }

        if (!((g_cursor_frame_count % (g_cursor_blink_on_frames * 2)) / g_cursor_blink_on_frames))
        {
            int cursor_x_offset = stb_easy_font_width_up_to(g_text_buffer.lines[g_cursor.line_i], g_cursor.char_i);
            int cursor_x = x_padding + x_line_num_offset + cursor_x_offset;
            int cursor_y = y_padding + line_height * g_cursor.line_i;
            int cursor_width = stb_easy_font_char_width(g_text_buffer.lines[g_cursor.line_i][g_cursor.char_i]);
            if (cursor_width == 0) cursor_width = 6;
            int cursor_height = 12;
            unsigned char color[] = { 200, 200, 200, 255 };
            draw_quad(cursor_x, cursor_y, cursor_width, cursor_height, color);
        }

        char line_i_str_buf[256];
        snprintf(line_i_str_buf, sizeof(line_i_str_buf),
            "STATUS: cursor: %d, %d; line len: %zu; invis: %d; lines: %d",
            g_cursor.line_i,
            g_cursor.char_i,
            strlen(g_text_buffer.lines[g_cursor.line_i]),
            g_debug_invis,
            g_text_buffer.line_count);
        unsigned char color[] = { 200, 200, 200, 255 };
        draw_string(10, 600 - line_height, line_i_str_buf, color);

        glfwSwapBuffers(window);
        glfwPollEvents();

        g_cursor_frame_count++;
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void)window;
    if (codepoint < 128) {
        insert_char(&g_text_buffer, (char)codepoint, &g_cursor);
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods;
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
    } else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i + 1, g_cursor.char_i);
    } else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i - 1, g_cursor.char_i);
    } else if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i, g_cursor.char_i - 1);
    } else if (key == GLFW_KEY_RIGHT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i, g_cursor.char_i + 1);
    } else if (key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        insert_char(&g_text_buffer, '\n', &g_cursor);
    } else if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        remove_char(&g_text_buffer, &g_cursor);
    } else if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        g_debug_invis = !g_debug_invis;
    } else if (key == GLFW_KEY_F12 && action == GLFW_PRESS) {
        __builtin_debugtrap();
    }
}

Text_Buffer read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    Text_Buffer result = {0};
    int count = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), f)) {
        result.lines = realloc(result.lines, (count + 1) * sizeof(char *));
        result.lines[count++] = strdup(buf);
    }
    result.line_count = count;
    fclose(f);
    return result;
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

void draw_content_string(int x, int y, char *string, unsigned char color[4])
{
    if (g_debug_invis) {
        string = strdup(string);
        convert_to_debug_invis(string);
    }

    draw_string(x, y, string, color);

    if (g_debug_invis) {
        free(string);
    }
}

void insert_char(Text_Buffer *text_buffer, char c, Cursor *cursor)
{
    if (c == '\n') {
        char *next_line = strdup(text_buffer->lines[cursor->line_i] + cursor->char_i);
        insert_line((&g_text_buffer), next_line, cursor->line_i + 1);
        text_buffer->lines[cursor->line_i] = realloc(text_buffer->lines[cursor->line_i], cursor->char_i + 2);
        text_buffer->lines[cursor->line_i][cursor->char_i] = '\n';
        text_buffer->lines[cursor->line_i][cursor->char_i + 1] = '\0';
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i + 1, 0);
    } else {
        size_t len = strlen(text_buffer->lines[cursor->line_i]);
        text_buffer->lines[cursor->line_i] = realloc(text_buffer->lines[cursor->line_i], len + 2);
        char *line = text_buffer->lines[cursor->line_i];
        for (int i = len; i >= cursor->char_i; i--) {
            line[i + 1] = line[i];
        }
        line[cursor->char_i] = c;
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i, g_cursor.char_i + 1);
    }
}

void remove_char(Text_Buffer *text_buffer, Cursor *cursor)
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
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i - 1, len0 - 1);
    } else {
        char *line = text_buffer->lines[cursor->line_i];
        size_t len = strlen(line);
        for (int i = cursor->char_i; i <= (int)len; i++) {
            line[i - 1] = line[i];
        }
        text_buffer->lines[cursor->line_i] = realloc(line, len);
        move_cursor(&g_text_buffer, &g_cursor, g_cursor.line_i, g_cursor.char_i - 1);
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

void move_cursor(Text_Buffer *text_buffer, Cursor *cursor, int line_i, int char_i)
{
    cursor->line_i = line_i;
    if (cursor->line_i < 0) {
        cursor->line_i = 0;
    }
    if (cursor->line_i >= text_buffer->line_count) {
        cursor->line_i = text_buffer->line_count - 1;
    }
    cursor->char_i = char_i;
    if (cursor->char_i < 0) {
        cursor->char_i = 0;
    }
    int line_len = strlen(text_buffer->lines[cursor->line_i]);
    if (cursor->char_i >= line_len) {
        cursor->char_i = line_len - 1;
    }
    g_cursor_frame_count = 0;
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
    for (int i = remove_at + 1; i < text_buffer->line_count - 1; i++) {
        text_buffer->lines[i - 1] = text_buffer->lines[i];
    }
    text_buffer->line_count--;
    text_buffer->lines = realloc(text_buffer->lines, text_buffer->line_count * sizeof(char *));
}
