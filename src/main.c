#include <stdio.h>
#include <string.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_easy_font.h>

#define VERT_MAX 4096

const char *vs_src =
"#version 410 core\n"
"layout (location = 0) in vec2 aPos;\n"
"void main() {\n"
"    vec2 zeroToOne = aPos / vec2(800, 600);\n"
"    vec2 zeroToTwo = zeroToOne * 2.0;\n"
"    vec2 clipSpace = zeroToTwo - 1.0;\n"
"    gl_Position = vec4(clipSpace.x, -clipSpace.y, 0.0, 1.0);\n"
"}";

const char *fs_src =
"#version 410 core\n"
"out vec4 FragColor;\n"
"void main() {\n"
"    FragColor = vec4(1.0, 0.5, 0.2, 1.0);\n"
"}";

typedef struct {
    char **lines;
    int line_count;
    int capacity;
} Text_Buffer;

typedef struct {
    float verts[VERT_MAX * 2];
    int vert_count;
} Vert_Buffer;

static int g_first_line = 0;
static int g_edit_line = 0;
static Text_Buffer g_text_buffer = {0};

void char_callback(GLFWwindow *window, unsigned int codepoint);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

Text_Buffer read_file(const char *path);
void render_line(int x, int y, char *line, Vert_Buffer *out_vert_buf);
void append_char_to_line(Text_Buffer *text_buffer, int cursor_line, char c);
void new_line(Text_Buffer *text_buffer, int cursor_line);
void remove_line(Text_Buffer *text_buffer, int cursor_line);

int main() {
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

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, 0);
    glCompileShader(vs);
    // {
    //     GLuint shader = vs;
    //     GLint success = 0;
    //     glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    //     if (!success) {
    //         char log[512];
    //         glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    //         fprintf(stderr, "Shader compile error:\n%s\nSource:\n", log);
    //         fprintf(stderr, "%s\n", vs_src);

    //     }
    // }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, 0);
    glCompileShader(fs);

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
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    g_text_buffer = read_file("res/mock.txt");

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);

        int y_offset = 10;
        int line_height = 20;

        for (int line_i = g_first_line; line_i < g_text_buffer.line_count; line_i++) {
            Vert_Buffer vert_buf = {0};
            render_line(10, y_offset, g_text_buffer.lines[line_i], &vert_buf);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * 2 * sizeof(float), vert_buf.verts);
            glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);

            y_offset += line_height;
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void char_callback(GLFWwindow *window, unsigned int codepoint) {
    (void)window;

    if (codepoint < 128) {
        append_char_to_line(&g_text_buffer, g_first_line, (char)codepoint);
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    (void)scancode; (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, 1);
    } else if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        g_edit_line++;
        if (g_edit_line >= g_text_buffer.line_count) {
            g_edit_line = g_text_buffer.line_count - 1;
        }
    } else if (key == GLFW_KEY_UP && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        g_edit_line--;
        if (g_edit_line < 0) {
            g_edit_line = 0;
        }
    } else if (key == GLFW_KEY_ENTER && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        new_line(&g_text_buffer, g_edit_line);
    } else if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        remove_line(&g_text_buffer, g_edit_line);
    }
}

Text_Buffer read_file(const char *path) {
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
    result.capacity = result.line_count;

    fclose(f);

    return result;
}

void render_line(int x, int y, char *line, Vert_Buffer *out_vert_buf) {
    char quad_buf[99999];
    int quad_count = stb_easy_font_print(x, y, line, NULL, quad_buf, sizeof(quad_buf));
    out_vert_buf->vert_count = quad_count * 6;

    // stb_easy_font outputs quads: 4 vertices per glyph box
    {
        float *src = (float *)quad_buf;

        for (int i = 0; i < quad_count; ++i) {
            float *q = &src[i * 4 * 4];

            float *dst = &out_vert_buf->verts[i * 6 * 2];

            // Triangle 1
            dst[0] = q[0]; dst[1] = q[1];
            dst[2] = q[4]; dst[3] = q[5];
            dst[4] = q[8]; dst[5] = q[9];

            // Triangle 2
            dst[6] = q[0];  dst[7] = q[1];
            dst[8] = q[8];  dst[9] = q[9];
            dst[10] = q[12]; dst[11] = q[13];
        }
    }
}

void append_char_to_line(Text_Buffer *text_buffer, int cursor_line, char c) {
    // printf("Append '%c' to line #%d '%s'\n", c, cursor_line, text_buffer->lines[g_first_line]);

    char *old_line = text_buffer->lines[cursor_line];
    size_t old_len = strlen(old_line);
    char *new_line = malloc(old_len + 2);
    memcpy(new_line, old_line, old_len);
    new_line[old_len - 1] = c;
    new_line[old_len] = '\n';
    new_line[old_len + 1] = '\0';
    free(old_line);
    text_buffer->lines[cursor_line] = new_line;
}

void new_line(Text_Buffer *text_buffer, int cursor_line) {
    text_buffer->lines = realloc(text_buffer->lines, (text_buffer->line_count + 1) * sizeof(char *));

    for (int i = text_buffer->line_count; i > cursor_line ; i--) {
        text_buffer->lines[i] = text_buffer->lines[i - 1];
    }

    text_buffer->lines[cursor_line + 1] = malloc(2);
    strcpy(text_buffer->lines[cursor_line + 1], "\n");

    text_buffer->line_count++;
    text_buffer->capacity = text_buffer->line_count;

    printf("added line #%d; line_count = %d; capacity = %d\n", cursor_line, text_buffer->line_count, text_buffer->capacity);
}

void remove_line(Text_Buffer *text_buffer, int cursor_line) {
    for (int i = cursor_line; i < text_buffer->line_count - 1; i++) {
        text_buffer->lines[i] = text_buffer->lines[i + 1];
    }

    if (text_buffer->line_count > 0) {
        free(text_buffer->lines[--text_buffer->line_count]);
        text_buffer->capacity = text_buffer->line_count;
    }

    printf("deleted line #%d; line_count = %d; capacity = %d\n", cursor_line, text_buffer->line_count, text_buffer->capacity);
}
