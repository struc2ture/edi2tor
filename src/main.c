#include <stdio.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_easy_font.h>


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

const float vertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f
};

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Test", NULL, NULL);
    if (!window) return -1;

    glfwMakeContextCurrent(window);

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

    char text[] = "HELLO WORLD!!!";
    char buffer[99999];
    float triangle_buffer[6 * 2 * 9999]; // 6 vertices (x, y) per quad
    int quad_count = stb_easy_font_print(10, 10, text, NULL, buffer, sizeof(buffer));
    int vert_count = quad_count * 6;

    // stb_easy_font outputs quads: 4 vertices per glyph box
    {
        float *src = (float *)buffer;

        for (int i = 0; i < quad_count; ++i) {
            float *q = &src[i * 4 * 4];

            float *dst = &triangle_buffer[i * 6 * 2];

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

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vert_count * 2 * sizeof(float), triangle_buffer, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    while (!glfwWindowShouldClose(window)) {
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(prog);
        glBindVertexArray(vao);
        glDrawArrays((GL_TRIANGLES), 0, vert_count);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
