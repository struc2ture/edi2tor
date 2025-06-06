#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "editor.h"

#include "shaders.h"
#include "util.h"

void _init(GLFWwindow *window, void *_state)
{
    (void)window; Editor_State *state = _state; (void)state;
    bassert(sizeof(*state) < 4096);
    glfwSetWindowUserPointer(window, _state);

    initialize_render_state(window, &state->render_state);

    state->canvas_viewport.zoom = 1.0f;
    viewport_set_outer_rect(&state->canvas_viewport, (Rect){0, 0, state->render_state.window_dim.x, state->render_state.window_dim.y});
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

    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    perform_timing_calculations(state);

    glUseProgram(state->render_state.grid_shader);
    draw_grid(state->canvas_viewport, &state->render_state);

    glUseProgram(state->render_state.main_shader);

    // Render buffer views backwards for correct z ordering
    for (int i = state->buffer_view_count - 1; i >= 0; i--)
    {
        Buffer_View *buffer_view = state->buffer_views[i];
        bool is_active = buffer_view == state->active_buffer_view;
        draw_buffer_view(buffer_view, is_active, state->canvas_viewport, &state->render_state, state->delta_time);
    }

    draw_status_bar(window, state, &state->render_state);

    handle_mouse_input(window, state);

    state->prev_mouse_pos = get_mouse_screen_pos(window);

    if (state->scroll_timeout > 0.0f)
    {
        state->scroll_timeout -= state->delta_time;
    }
    else
    {
        state->scrolled_buffer_view = NULL;
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    state->frame_count++;
    state->fps_frame_count++;
}

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window);
    if (codepoint < 128) {
        handle_char_input(state, (char)codepoint);
    }
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods; Editor_State *state = glfwGetWindowUserPointer(window);
    handle_key_input(window, state, key, action, mods);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    (void)window; (void)button; (void)action; (void)mods; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
}

void scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window; (void)x_offset; (void)y_offset; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    if (state->scroll_timeout <= 0.0f)
    {
        Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
        Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
        state->scrolled_buffer_view = get_buffer_view_at_pos(mouse_canvas_pos, state);
    }
    state->scroll_timeout = 0.1f;
    if (state->scrolled_buffer_view)
    {
        state->scrolled_buffer_view->viewport.rect.x -= x_offset * SCROLL_SENS;
        state->scrolled_buffer_view->viewport.rect.y -= y_offset * SCROLL_SENS;

        if (state->scrolled_buffer_view->viewport.rect.x < 0.0f) state->scrolled_buffer_view->viewport.rect.x = 0.0f;
        if (state->scrolled_buffer_view->viewport.rect.y < 0.0f) state->scrolled_buffer_view->viewport.rect.y = 0.0f;
        float buffer_max_y = (state->scrolled_buffer_view->buffer->text_buffer.line_count - 1) * get_font_line_height(state->render_state.font);
        if (state->scrolled_buffer_view->viewport.rect.y > buffer_max_y) state->scrolled_buffer_view->viewport.rect.y = buffer_max_y;
        float buffer_max_x = 256 * get_font_line_height(state->render_state.font); // TODO: Determine max x coordinates based on longest line
        if (state->scrolled_buffer_view->viewport.rect.x > buffer_max_x) state->scrolled_buffer_view->viewport.rect.x = buffer_max_x;
    }
    else
    {
        state->canvas_viewport.rect.x -= x_offset * SCROLL_SENS;
        state->canvas_viewport.rect.y -= y_offset * SCROLL_SENS;
    }
}

void framebuffer_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window; (void)w; (void)h; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    glViewport(0, 0, w, h);
    state->render_state.framebuffer_dim.x = w;
    state->render_state.framebuffer_dim.y = h;
    state->render_state.dpi_scale = state->render_state.framebuffer_dim.x / state->render_state.window_dim.x;
}

void window_size_callback(GLFWwindow *window, int w, int h)
{
    (void)window; (void)w; (void)h; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    state->render_state.window_dim.x = w;
    state->render_state.window_dim.y = h;
    state->render_state.dpi_scale = state->render_state.framebuffer_dim.x / state->render_state.window_dim.x;
}

void refresh_callback(GLFWwindow* window) {
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    // TODO: This seems to cause an issue with window resize action not ending in macos, main program loop still being blocked
    // _render(window, state);
}

bool gl_check_compile_success(GLuint shader, const char *src)
{
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error:\n%s\nSource:\n", log);
        fprintf(stderr, "%s\n", src);
    }
    return (bool)success;
}

bool gl_check_link_success(GLuint prog)
{
    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
    }
    return (bool)success;
}

GLuint gl_create_shader_program(const char *vs_src, const char *fs_src)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, 0);
    glCompileShader(vs);
    gl_check_compile_success(vs, vs_src);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, 0);
    glCompileShader(fs);
    gl_check_compile_success(fs, fs_src);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    gl_check_link_success(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

void gl_enable_scissor(Rect screen_rect, Render_State *render_state)
{
    glEnable(GL_SCISSOR_TEST);
    Rect scaled_rect = {
        .x = screen_rect.x * render_state->dpi_scale,
        .y = screen_rect.y * render_state->dpi_scale,
        .w = screen_rect.w * render_state->dpi_scale,
        .h = screen_rect.h * render_state->dpi_scale
    };
    float scaled_window_h = render_state->window_dim.y * render_state->dpi_scale;
    GLint scissor_x = (GLint)floor(scaled_rect.x);
    float screen_rect_topdown_bottom_y = scaled_rect.y + scaled_rect.h;
    float screen_rect_bottomup_bottom_y = scaled_window_h - screen_rect_topdown_bottom_y;
    GLint scissor_y = (GLint)floor(screen_rect_bottomup_bottom_y);
    GLsizei scissor_w = (GLsizei)(ceil(scaled_rect.x + scaled_rect.w) - scissor_x);
    GLsizei scissor_h = (GLsizei)(ceil(screen_rect_bottomup_bottom_y + scaled_rect.h) - scissor_y);
    glScissor(scissor_x, scissor_y, scissor_w, scissor_h);
}

void initialize_render_state(GLFWwindow *window, Render_State *render_state)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int framebuffer_w, framebuffer_h;
    glfwGetFramebufferSize(window, &framebuffer_w, &framebuffer_h);
    glViewport(0, 0, framebuffer_w, framebuffer_h);

    render_state->main_shader = gl_create_shader_program(shader_main_vert_src, shader_main_frag_src);
    render_state->grid_shader = gl_create_shader_program(shader_main_vert_src, shader_grid_frag_src);

    render_state->grid_shader_mvp_loc = glGetUniformLocation(render_state->grid_shader, "u_mvp");
    render_state->grid_shader_offset_loc = glGetUniformLocation(render_state->grid_shader, "u_offset");
    render_state->grid_shader_resolution_loc = glGetUniformLocation(render_state->grid_shader, "u_resolution");
    render_state->main_shader_mvp_loc = glGetUniformLocation(render_state->main_shader, "u_mvp");

    glGenVertexArrays(1, &render_state->vao);
    glGenBuffers(1, &render_state->vbo);
    glBindVertexArray(render_state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, render_state->vbo);
    glBufferData(GL_ARRAY_BUFFER,  VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (void *)offsetof(Vert, r));
    glEnableVertexAttribArray(2);

    render_state->font = load_font(FONT_PATH);
    render_state->buffer_view_line_num_col_width = get_string_rect("000", render_state->font, 0, 0).w;
    render_state->buffer_view_name_height = get_font_line_height(render_state->font);
    render_state->buffer_view_padding = 6.0f;
    render_state->buffer_view_resize_handle_radius = 5.0f;

    int window_w, window_h;
    glfwGetWindowSize(window, &window_w, &window_h);
    render_state->window_dim.x = window_w;
    render_state->window_dim.y = window_h;
    render_state->framebuffer_dim.x = framebuffer_w;
    render_state->framebuffer_dim.y = framebuffer_h;
    render_state->dpi_scale = render_state->framebuffer_dim.x / render_state->window_dim.x;
}

void perform_timing_calculations(Editor_State *state)
{
    float current_time = (float)glfwGetTime();
    state->delta_time = current_time - state->last_frame_time;
    state->last_frame_time = current_time;

    if (current_time - state->last_fps_time > 0.1f)
    {
        state->fps = (float)state->fps_frame_count / (current_time - state->last_fps_time);
        state->last_fps_time = current_time;
        state->fps_frame_count = 0;
    }
}

Buffer **buffer_create_new_slot(Editor_State *state)
{
    state->buffer_count++;
    state->buffers = xrealloc(state->buffers, state->buffer_count * sizeof(state->buffers[0]));
    return &state->buffers[state->buffer_count - 1];
}

void buffer_free_slot(Buffer *buffer, Editor_State *state)
{
    int index_to_delete = buffer_get_index(buffer, state);
    for (int i = index_to_delete; i < state->buffer_count - 1; i++)
    {
        state->buffers[i] = state->buffers[i + 1];
    }
    state->buffer_count--;
    state->buffers = xrealloc(state->buffers, state->buffer_count * sizeof(state->buffers[0]));
}

Buffer *buffer_create_read_file(const char *path, Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);

    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->kind = BUFFER_FILE;
    buffer->file.info = file_read_into_text_buffer(path, &buffer->text_buffer);

    *new_slot = buffer;
    return *new_slot;
}

Buffer *buffer_create_prompt(const char *prompt_text, Prompt_Context context, Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);

    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->kind = BUFFER_PROMPT;

    buffer->text_buffer.line_count = 2;
    buffer->text_buffer.lines = xmalloc(buffer->text_buffer.line_count * sizeof(buffer->text_buffer.lines[0]));
    buffer->prompt.context = context;

    char prompt_line_buf[MAX_CHARS_PER_LINE];
    snprintf(prompt_line_buf, sizeof(prompt_line_buf), "%s\n", prompt_text);
    buffer->text_buffer.lines[0] = make_text_line_dup(prompt_text);
    buffer->text_buffer.lines[1] = make_text_line_dup("\n");

    *new_slot = buffer;
    return *new_slot;
}

int buffer_get_index(Buffer *buffer, Editor_State *state)
{
    int index = 0;
    Buffer **buffer_c = state->buffers;
    while (*buffer_c != buffer)
    {
        index++;
        buffer_c++;
    }
    return index;
}

void buffer_destroy(Buffer *buffer, Editor_State *state)
{
    switch (buffer->kind)
    {
        case BUFFER_FILE:
        {
            text_buffer_destroy(&buffer->text_buffer);
            buffer_free_slot(buffer, state);
            free(buffer);
        } break;

        case BUFFER_PROMPT:
        {
            text_buffer_destroy(&buffer->text_buffer);
            buffer_free_slot(buffer, state);
            free(buffer);
        } break;

        default:
        {
            bassert(!"buffer_destroy: unhandled buffer kind.");
        }
    }
}

Buffer_View *buffer_view_create(Rect rect, Editor_State *state)
{
    Buffer_View *view = xcalloc(sizeof(Buffer_View));
    view->viewport.zoom = DEFAULT_ZOOM;
    buffer_view_set_rect(view, rect, &state->render_state);
    state->buffer_view_count++;
    state->buffer_views = xrealloc(state->buffer_views, state->buffer_view_count * sizeof(state->buffer_views[0]));
    Buffer_View **new_slot = &state->buffer_views[state->buffer_view_count - 1];
    *new_slot = view;
    return *new_slot;
}

Buffer_View *buffer_view_open_file(const char *file_path, Rect rect, Editor_State *state)
{
    Buffer_View *new_view = buffer_view_create(rect, state);
    Buffer *new_buffer = buffer_create_read_file(file_path, state);
    new_view->buffer = new_buffer;
    move_cursor_to_line(new_view, &new_buffer->text_buffer, &new_view->cursor, state, new_view->cursor.pos.line, false);
    move_cursor_to_col(new_view, &new_buffer->text_buffer, &new_view->cursor, state, new_view->cursor.pos.col, false, false);
    buffer_view_set_active(new_view, state);
    return new_view;
}

Buffer_View *buffer_view_prompt(const char *prompt_text, Prompt_Context context, Rect rect, Editor_State *state)
{
    Buffer_View *new_view = buffer_view_create(rect, state);
    Buffer *new_buffer = buffer_create_prompt(prompt_text, context, state);
    new_view->buffer = new_buffer;
    move_cursor_to_line(new_view, &new_buffer->text_buffer, &new_view->cursor, state, 1, false);
    move_cursor_to_col(new_view, &new_buffer->text_buffer, &new_view->cursor, state, 0, false, false);
    buffer_view_set_active(new_view, state);
    return new_view;
}

void buffer_view_destroy(Buffer_View *buffer_view, Editor_State *state)
{
    buffer_destroy(buffer_view->buffer, state);
    int index_to_delete = buffer_view_get_index(buffer_view, state);
    free(state->buffer_views[index_to_delete]);
    for (int i = index_to_delete; i < state->buffer_view_count - 1; i++)
    {
        state->buffer_views[i] = state->buffer_views[i + 1];
    }
    state->buffer_view_count--;
    state->buffer_views = xrealloc(state->buffer_views, state->buffer_view_count * sizeof(state->buffer_views[0]));
    if (state->active_buffer_view == buffer_view)
    {
        state->active_buffer_view = state->buffer_views[0];
    }
}

int buffer_view_get_index(Buffer_View *buffer_view, Editor_State *state)
{
    int index = 0;
    Buffer_View **buffer_view_c = state->buffer_views;
    while (*buffer_view_c != buffer_view)
    {
        index++;
        buffer_view_c++;
    }
    return index;
}

bool buffer_view_exists(Buffer_View *buffer_view, Editor_State *state)
{
    for (int i = 0; i < state->buffer_view_count; i++)
    {
        if (state->buffer_views[i] == buffer_view)
            return true;
    }
    return false;
}

void buffer_view_set_active(Buffer_View *buffer_view, Editor_State *state)
{
    int active_index = buffer_view_get_index(buffer_view, state);
    for (int i = active_index; i > 0; i--)
    {
        state->buffer_views[i] = state->buffer_views[i - 1];
    }
    state->buffer_views[0] = buffer_view;
    state->active_buffer_view = buffer_view;
}

Rect buffer_view_get_text_area_rect(Buffer_View buffer_view, const Render_State *render_state)
{
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect r;
    r.x = buffer_view.outer_rect.x + pad + line_num_w + pad;
    r.y = buffer_view.outer_rect.y + pad + name_h + pad;
    r.w = buffer_view.outer_rect.w - pad - line_num_w - pad - pad;
    r.h = buffer_view.outer_rect.h - pad - name_h - pad - pad;
    return r;
}

Rect buffer_view_get_line_num_col_rect(Buffer_View buffer_view, const Render_State *render_state)
{
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect r;
    r.x = buffer_view.outer_rect.x + pad;
    r.y = buffer_view.outer_rect.y + pad + name_h + pad;
    r.w = line_num_w;
    r.h = buffer_view.outer_rect.h - pad - name_h - pad - pad;
    return r;
}

Rect buffer_view_get_name_rect(Buffer_View buffer_view, const Render_State *render_state)
{
    const float right_limit = 40.0f;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect r;
    r.x = buffer_view.outer_rect.x + pad + line_num_w + pad;
    r.y = buffer_view.outer_rect.y + pad;
    r.w = buffer_view.outer_rect.w - pad - line_num_w - pad - pad - right_limit;
    r.h = name_h;
    return r;
}

Rect buffer_view_get_resize_handle_rect(Buffer_View buffer_view, const Render_State *render_state)
{
    Rect r;
    r.x = buffer_view.outer_rect.x + buffer_view.outer_rect.w - render_state->buffer_view_resize_handle_radius;
    r.y = buffer_view.outer_rect.y + buffer_view.outer_rect.h - render_state->buffer_view_resize_handle_radius;
    r.w = 2 * render_state->buffer_view_resize_handle_radius;
    r.h = 2 * render_state->buffer_view_resize_handle_radius;
    return r;
}

void buffer_view_set_rect(Buffer_View *buffer_view, Rect rect, const Render_State *render_state)
{
    buffer_view->outer_rect = rect;
    Rect text_area_rect = buffer_view_get_text_area_rect(*buffer_view, render_state);
    viewport_set_outer_rect(&buffer_view->viewport, text_area_rect);
}

Vec_2 buffer_view_canvas_pos_to_text_area_pos(Buffer_View buffer_view, Vec_2 canvas_pos, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, render_state);
    Vec_2 text_area_pos;
    text_area_pos.x = canvas_pos.x - text_area_rect.x;
    text_area_pos.y = canvas_pos.y - text_area_rect.y;
    return text_area_pos;
}

Vec_2 buffer_view_text_area_pos_to_buffer_pos(Buffer_View buffer_view, Vec_2 text_area_pos)
{
    Vec_2 buffer_pos;
    buffer_pos.x = buffer_view.viewport.rect.x + text_area_pos.x / buffer_view.viewport.zoom;
    buffer_pos.y = buffer_view.viewport.rect.y + text_area_pos.y / buffer_view.viewport.zoom;
    return buffer_pos;
}

Prompt_Context prompt_create_context_open_file()
{
    Prompt_Context context;
    context.kind = PROMPT_OPEN_FILE;
    return context;
}

Prompt_Context prompt_create_context_go_to_line(Buffer_View *for_view)
{
    Prompt_Context context;
    context.kind = PROMPT_GO_TO_LINE;
    context.go_to_line.for_view = for_view;
    return context;
}

Prompt_Context prompt_create_context_save_as(Buffer_View *for_view)
{
    Prompt_Context context;
    context.kind = PROMPT_SAVE_AS;
    context.go_to_line.for_view = for_view;
    return context;
}

Prompt_Result prompt_parse_result(Text_Buffer text_buffer)
{
    bassert(text_buffer.line_count >= 2);
    bassert(text_buffer.lines[1].buf_len < MAX_CHARS_PER_LINE);
    Prompt_Result result;
    strcpy(result.str, text_buffer.lines[1].str);
    if (result.str[text_buffer.lines[1].len - 1] == '\n')
    {
        result.str[text_buffer.lines[1].len - 1] = '\0';
    }
    return result;
}

void prompt_submit(Prompt_Context context, Prompt_Result result, Rect prompt_rect, GLFWwindow *window, Editor_State *state)
{
    (void)window; (void)state;
    switch (context.kind)
    {
        case PROMPT_OPEN_FILE:
        {
            Rect buffer_view_rect =
            {
                .x = prompt_rect.x,
                .y = prompt_rect.y,
                .w = 500,
                .h = 500
            };
            buffer_view_open_file(result.str, buffer_view_rect, state);
        } break;

        case PROMPT_GO_TO_LINE:
        {
            if (buffer_view_exists(context.go_to_line.for_view, state))
            {
                int go_to_line = xstrtoint(result.str);
                display_cursor_move(context.go_to_line.for_view, (Cursor_Pos){go_to_line - 1, 0}, state);
            }
            else log_warning("prompt_submit: PROMPT_GO_TO_LINE: Buffer %p does not exist", context.go_to_line.for_view);
        } break;

        case PROMPT_SAVE_AS:
        {
            if (buffer_view_exists(context.go_to_line.for_view, state))
            {
                file_write(context.go_to_line.for_view->buffer->text_buffer, result.str);
            }
        } break;

        default:
        {
            log_warning("prompt_submit: unhandled prompt kind");
        } break;
    }
}

void viewport_set_outer_rect(Viewport *viewport, Rect outer_rect)
{
    viewport->rect.w = outer_rect.w / viewport->zoom;
    viewport->rect.h = outer_rect.h / viewport->zoom;
}

void viewport_set_zoom(Viewport *viewport, float new_zoom)
{
    viewport->rect.w *= viewport->zoom / new_zoom;
    viewport->rect.h *= viewport->zoom / new_zoom;
    viewport->zoom = new_zoom;
}

Vert make_vert(float x, float y, float u, float v, const unsigned char color[4])
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

float get_font_line_height(Render_Font font)
{
    float height = font.ascent - font.descent;
    return height;
}

float get_char_width(char c, Render_Font font)
{
    float char_width = font.char_data[c - 32].xadvance;
    return char_width;
}

Rect get_string_rect(const char *str, Render_Font font, float x, float y)
{
    Rect r;
    r.x = x;
    r.y = y;
    r.w = 0;
    r.h = get_font_line_height(font);
    while (*str)
    {
        r.w += get_char_width(*str, font);
        str++;
    }
    return r;
}

Rect get_string_range_rect(const char *str, Render_Font font, int start_char, int end_char)
{
    bassert(start_char >= 0);
    bassert(start_char < end_char);
    Rect r;
    r.y = 0;
    r.h = get_font_line_height(font);
    float x = 0, min_x = 0, max_x = 0;
    for (int i = 0; i < end_char && str[i]; i++)
    {
        if (i == start_char)
        {
            min_x = x;
        }

        char c = str[i];
        if (str[i + 1 == '\0'] && c == '\n')
        {
            c = ' ';
        }
        x += get_char_width(c, font);
    }
    // If reached end_char index, x will be max_x (end_char itself is not included)
    // If reached null terminator, it will still be valid, highlighting the whole string.
    max_x = x;
    r.x = min_x;
    r.w = max_x - min_x;
    return r;
}

Rect get_string_char_rect(const char *str, Render_Font font, int char_i)
{
    Rect r = {0};
    r.h = get_font_line_height(font);
    float x = 0, min_x = 0, max_x = 0;
    int i = 0;
    bool found_max = false;
    while (*str)
    {
        char c = *str;
        if (c == '\n')
        {
            c = ' ';
        }
        x += get_char_width(c, font);
        if (i == char_i)
        {
            max_x = x;
            found_max = true;
            break;
        }
        else
        {
            min_x = x;
        }
        str++;
        i++;
    }
    bassert(found_max);
    r.x = min_x;
    r.w = max_x - min_x;
    return r;
}

int get_char_i_at_pos_in_string(const char *str, Render_Font font, float x)
{
    int char_i = 0;
    float str_x = 0;
    while (*str)
    {
        str_x += get_char_width(*str, font);
        if (str_x > x)
        {
            return char_i;
        }
        char_i++;
        str++;
    }
    return char_i;
}

Rect get_cursor_rect(Text_Buffer text_buffer, Display_Cursor cursor, Render_State *render_state)
{
    Rect cursor_rect = get_string_char_rect(text_buffer.lines[cursor.pos.line].str, render_state->font, cursor.pos.col);
    float line_height = get_font_line_height(render_state->font);
    float x = 0;
    float y = cursor.pos.line * line_height;
    cursor_rect.x += x;
    cursor_rect.y += y;
    return cursor_rect;
}

void draw_string(const char *str, Render_Font font, float x, float y, const unsigned char color[4])
{
    y += font.ascent;
    Vert_Buffer vert_buf = {0};
    while (*str)
    {
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font.char_data, font.atlas_w, font.atlas_h, *str-32, &x, &y ,&q, 1);
        vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y0, q.s0, q.t0, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, color));
        vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y1, q.s1, q.t1, color));
        str++;
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font.texture);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, font.white_texture);
}

void draw_quad(Rect q, const unsigned char color[4])
{
    Vert_Buffer vert_buf = {0};
    vert_buf.vert_count = 6;
    vert_buf.verts[0].x = 0;     vert_buf.verts[0].y = 0;
    vert_buf.verts[1].x = 0;     vert_buf.verts[1].y = q.h;
    vert_buf.verts[2].x = q.w; vert_buf.verts[2].y = 0;
    vert_buf.verts[3].x = q.w; vert_buf.verts[3].y = 0;
    vert_buf.verts[4].x = 0;     vert_buf.verts[4].y = q.h;
    vert_buf.verts[5].x = q.w; vert_buf.verts[5].y = q.h;
    for (int i = 0; i < 6; i++) {
        vert_buf.verts[i].x += q.x;
        vert_buf.verts[i].y += q.y;
        vert_buf.verts[i].r = color[0];
        vert_buf.verts[i].g = color[1];
        vert_buf.verts[i].b = color[2];
        vert_buf.verts[i].a = color[3];
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_grid(Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16];
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    glUniformMatrix4fv(render_state->grid_shader_mvp_loc, 1, GL_FALSE, proj);

    glUniform2f(render_state->grid_shader_resolution_loc, render_state->framebuffer_dim.x, render_state->framebuffer_dim.y);

    float scaled_offset_x = canvas_viewport.rect.x * render_state->dpi_scale;
    float scaled_offset_y = canvas_viewport.rect.y * render_state->dpi_scale;
    glUniform2f(render_state->grid_shader_offset_loc, scaled_offset_x, scaled_offset_y);

    draw_quad((Rect){0, 0, render_state->window_dim.x, render_state->window_dim.y}, (unsigned char[4]){0});
}

void draw_text_buffer(Text_Buffer text_buffer, Viewport viewport, Render_State *render_state)
{
    float x = 0, y = 0;
    float line_height = get_font_line_height(render_state->font);

    for (int i = 0; i < text_buffer.line_count; i++)
    {
        Rect string_rect = get_string_rect(text_buffer.lines[i].str, render_state->font, 0, y);
        bool is_seen = rect_intersect(string_rect, viewport.rect);
        if (is_seen)
            draw_string(text_buffer.lines[i].str, render_state->font, x, y, (unsigned char[4]){255, 255, 255, 255});
        y += line_height;
    }
}

void draw_cursor(Text_Buffer text_buffer, Display_Cursor *cursor, Viewport viewport, Render_State *render_state, float delta_time)
{
    cursor->blink_time += delta_time;
    if (cursor->blink_time < 0.5f)
    {
        Rect cursor_rect = get_cursor_rect(text_buffer, *cursor, render_state);
        bool is_seen = rect_intersect(cursor_rect, viewport.rect);
        if (is_seen)
            draw_quad(cursor_rect, (unsigned char[4]){0, 0, 255, 255});
    } else if (cursor->blink_time > 1.0f)
    {
        cursor->blink_time -= 1.0f;
    }
}

void draw_selection(Text_Buffer text_buffer, Text_Selection selection, Viewport viewport, Render_State *render_state)
{
    if (is_selection_valid(text_buffer, selection)) {
        Cursor_Pos start = selection.start;
        Cursor_Pos end = selection.end;
        if (start.line > end.line || (start.line == end.line && start.col > end.col)) {
            Cursor_Pos tmp = start;
            start = end;
            end = tmp;
        }
        for (int i = start.line; i <= end.line; i++)
        {
            Text_Line *line = &text_buffer.lines[i];
            int h_start, h_end;
            if (i == start.line && i == end.line) {
                h_start = start.col;
                h_end = end.col;
            } else if (i == start.line) {
                h_start = start.col;
                h_end = line->len;
            } else if (i == end.line) {
                h_start = 0;
                h_end = end.col;
            } else {
                h_start = 0;
                h_end = line->len;
            }
            if (h_end > h_start)
            {
                Rect selected_rect = get_string_range_rect(line->str, render_state->font, h_start, h_end);
                selected_rect.y += i * get_font_line_height(render_state->font);
                if (rect_intersect(selected_rect, viewport.rect))
                    draw_quad(selected_rect, (unsigned char[4]){200, 200, 200, 130});
            }
        }
    }
}

void draw_line_numbers(Buffer_View buffer_view, Viewport canvas_viewport, Render_State *render_state)
{
    transform_set_buffer_view_line_num_col(buffer_view, canvas_viewport, render_state);

    const float font_line_height = get_font_line_height(render_state->font);
    const float viewport_min_y = buffer_view.viewport.rect.y;
    const float viewport_max_y = viewport_min_y + buffer_view.viewport.rect.h;

    char line_i_str_buf[256];
    for (int line_i = 0; line_i < buffer_view.buffer->text_buffer.line_count; line_i++)
    {
        const float min_y = font_line_height * line_i;
        const float max_y = min_y + font_line_height;
        if (min_y < viewport_max_y && max_y > viewport_min_y)
        {
            snprintf(line_i_str_buf, sizeof(line_i_str_buf), "%3d", line_i + 1);
            unsigned char color[4];
            if (line_i != buffer_view.cursor.pos.line)
            {
                color[0] = 150; color[1] = 150; color[2] = 150; color[3] = 255;
            }
            else
            {
                color[0] = 230; color[1] = 230; color[2] = 230; color[3] = 255;
            }
            draw_string(line_i_str_buf, render_state->font, 0, min_y, color);
        }
    }
}

void draw_buffer_view_name(Buffer_View buffer_view, bool is_active, Viewport canvas_viewport, Render_State *render_state)
{
    Rect name_rect = buffer_view_get_name_rect(buffer_view, render_state);

    char buffer_view_name_buf[256];
    if (!buffer_view.buffer->file.info.has_been_modified)
        snprintf(buffer_view_name_buf, sizeof(buffer_view_name_buf), "%s", buffer_view.buffer->file.info.path);
    else
        snprintf(buffer_view_name_buf, sizeof(buffer_view_name_buf), "%s[*]", buffer_view.buffer->file.info.path);

    transform_set_rect(name_rect, canvas_viewport, render_state);
    if (is_active)
        draw_string(buffer_view_name_buf, render_state->font, 0, 0, (unsigned char[4]){140, 140, 140, 255});
    else
        draw_string(buffer_view_name_buf, render_state->font, 0, 0, (unsigned char[4]){100, 100, 100, 255});
}

void draw_buffer_view(Buffer_View *buffer_view, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time)
{
    transform_set_canvas_space(canvas_viewport, render_state);
    if (is_active)
        draw_quad(buffer_view->outer_rect, (unsigned char[4]){40, 40, 40, 255});
    else
        draw_quad(buffer_view->outer_rect, (unsigned char[4]){20, 20, 20, 255});

    Rect text_area_rect = buffer_view_get_text_area_rect(*buffer_view, render_state);
    draw_quad(text_area_rect, (unsigned char[4]){10, 10, 10, 255});

    transform_set_buffer_view_text_area(*buffer_view, canvas_viewport, render_state);
    draw_text_buffer(buffer_view->buffer->text_buffer, buffer_view->viewport, render_state);
    if (is_active)
        draw_cursor(buffer_view->buffer->text_buffer, &buffer_view->cursor, buffer_view->viewport, render_state, delta_time);
    draw_selection(buffer_view->buffer->text_buffer, buffer_view->selection, buffer_view->viewport, render_state);
    draw_line_numbers(*buffer_view, canvas_viewport, render_state);
    if (buffer_view->buffer->kind == BUFFER_FILE)
    {
        draw_buffer_view_name(*buffer_view, is_active, canvas_viewport, render_state);
    }
}

void draw_status_bar(GLFWwindow *window, Editor_State *state, Render_State *render_state)
{
    (void)window;
    transform_set_screen_space(render_state);

    const float font_line_height = get_font_line_height(render_state->font);
    const float x_padding = 10;
    const float y_padding = 2;
    const float status_bar_height = 2 * font_line_height + 2 * y_padding;

    const Rect status_bar_rect = {
        .x = 0,
        .y = render_state->window_dim.y - status_bar_height,
        .w = render_state->window_dim.x,
        .h = render_state->window_dim.y};

    draw_quad(status_bar_rect, (unsigned char[4]){30, 30, 30, 255});

    char status_str_buf[256];
    const unsigned char status_str_color[] = { 200, 200, 200, 255 };

    float status_str_x = status_bar_rect.x + x_padding;
    float status_str_y = status_bar_rect.y + y_padding;

    Buffer_View *active_view = state->active_buffer_view;
    if (active_view)
    {
        snprintf(status_str_buf, sizeof(status_str_buf),
            "STATUS: Cursor: %d, %d; Line Len: %d; Lines: %d",
            active_view->cursor.pos.line,
            active_view->cursor.pos.col,
            active_view->buffer->text_buffer.lines[active_view->cursor.pos.line].len,
            active_view->buffer->text_buffer.line_count);
        draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color);
        status_str_y += font_line_height;
    }

    snprintf(status_str_buf, sizeof(status_str_buf), "FPS: %3.0f; Delta: %.3f", state->fps, state->delta_time);
    draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color);
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

void make_viewport_transform(Viewport viewport, float *out)
{
    make_view(viewport.rect.x, viewport.rect.y, viewport.zoom, out);
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

void make_mat4_identity(float *out)
{
    out[0]  = 1;  out[1]  = 0;  out[2]  = 0;  out[3]  = 0;
    out[4]  = 0;  out[5]  = 1;  out[6]  = 0;  out[7]  = 0;
    out[8]  = 0;  out[9]  = 0;  out[10] = 1;  out[11] = 0;
    out[12] = 0;  out[13] = 0;  out[14] = 0;  out[15] = 1;
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

void transform_set_buffer_view_text_area(Buffer_View buffer_view, Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16], view_viewport[16], view_canvas[16], view_a[16], view_screen[16], view[16], mvp[16];

    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, render_state);

    make_viewport_transform(buffer_view.viewport, view_viewport);
    make_view(-text_area_rect.x, -text_area_rect.y, 1.0f, view_canvas);
    make_viewport_transform(canvas_viewport, view_screen);
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    mul_mat4(view_viewport, view_canvas, view_a);
    mul_mat4(view_a, view_screen, view);
    mul_mat4(view, proj, mvp);

    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, mvp);

    Rect text_area_screen_rect = canvas_rect_to_screen_rect(text_area_rect, canvas_viewport);
    gl_enable_scissor(text_area_screen_rect, render_state);
}

void transform_set_buffer_view_line_num_col(Buffer_View buffer_view, Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16], view_viewport[16], view_canvas[16], view_a[16], view_screen[16], view[16], mvp[16];

    Rect line_num_col_rect = buffer_view_get_line_num_col_rect(buffer_view, render_state);

    make_view(0.0f, buffer_view.viewport.rect.y, buffer_view.viewport.zoom, view_viewport);
    make_view(-line_num_col_rect.x, -line_num_col_rect.y, 1.0f, view_canvas);
    make_viewport_transform(canvas_viewport, view_screen);
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    mul_mat4(view_viewport, view_canvas, view_a);
    mul_mat4(view_a, view_screen, view);
    mul_mat4(view, proj, mvp);

    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, mvp);

    Rect line_num_col_screen_rect = canvas_rect_to_screen_rect(line_num_col_rect, canvas_viewport);
    gl_enable_scissor(line_num_col_screen_rect, render_state);
}

void transform_set_rect(Rect rect, Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16], view_canvas[16], view_screen[16], view[16], mvp[16];

    make_view(-rect.x, - rect.y, 1.0f, view_canvas);
    make_viewport_transform(canvas_viewport, view_screen);
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    mul_mat4(view_canvas, view_screen, view);
    mul_mat4(view, proj, mvp);

    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, mvp);

    Rect screen_rect = canvas_rect_to_screen_rect(rect, canvas_viewport);
    gl_enable_scissor(screen_rect, render_state);
}

void transform_set_canvas_space(Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16], view[16], mvp[16];

    make_viewport_transform(canvas_viewport, view);
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    mul_mat4(view, proj, mvp);

    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, mvp);

    glDisable(GL_SCISSOR_TEST);
}

void transform_set_screen_space(Render_State *render_state)
{
    float proj[16];

    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, proj);

    glDisable(GL_SCISSOR_TEST);
}

Rect_Bounds get_viewport_cursor_bounds(Viewport viewport, Render_Font font)
{
    Rect_Bounds viewport_bounds = rect_get_bounds(viewport.rect);
    float space_width = get_char_width(' ', font);
    float font_line_height = get_font_line_height(font);
    viewport_bounds.min_x += VIEWPORT_CURSOR_BOUNDARY_COLUMNS * space_width;
    viewport_bounds.max_x -= VIEWPORT_CURSOR_BOUNDARY_COLUMNS * space_width;
    viewport_bounds.min_y += VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
    viewport_bounds.max_y -= VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
    return viewport_bounds;
}

Rect canvas_rect_to_screen_rect(Rect canvas_rect, Viewport canvas_viewport)
{
    Rect screen_rect;
    screen_rect.x = canvas_rect.x - canvas_viewport.rect.x;
    screen_rect.y = canvas_rect.y - canvas_viewport.rect.y;
    screen_rect.w = canvas_rect.w * canvas_viewport.zoom;
    screen_rect.h = canvas_rect.h * canvas_viewport.zoom;
    return screen_rect;
}

Vec_2 screen_pos_to_canvas_pos(Vec_2 screen_pos, Viewport canvas_viewport)
{
    Vec_2 canvas_pos;
    canvas_pos.x = canvas_viewport.rect.x + screen_pos.x / canvas_viewport.zoom;
    canvas_pos.y = canvas_viewport.rect.y + screen_pos.y / canvas_viewport.zoom;
    return canvas_pos;
}

Vec_2 get_mouse_screen_pos(GLFWwindow *window)
{
    double mouse_x, mouse_y;
    glfwGetCursorPos(window, &mouse_x, &mouse_y);
    Vec_2 p = {mouse_x, mouse_y};
    return p;
}

Vec_2 get_mouse_canvas_pos(GLFWwindow *window, Editor_State *state)
{
    Vec_2 p = screen_pos_to_canvas_pos(get_mouse_screen_pos(window), state->canvas_viewport);
    return p;
}

Vec_2 get_mouse_delta(GLFWwindow *window, Editor_State *state)
{
    Vec_2 current_mouse_pos = get_mouse_screen_pos(window);
    Vec_2 delta_mouse_pos = {
        .x = current_mouse_pos.x - state->prev_mouse_pos.x,
        .y = current_mouse_pos.y - state->prev_mouse_pos.y
    };
    return delta_mouse_pos;
}

Buffer_View *get_buffer_view_at_pos(Vec_2 pos, Editor_State *state)
{
    for (int i = 0; i < state->buffer_view_count; i++)
    {
        Rect buffer_view_rect = state->buffer_views[i]->outer_rect;
        Rect resize_handle_rect = buffer_view_get_resize_handle_rect(*state->buffer_views[i], &state->render_state);
        bool at_buffer_view_rect = rect_p_intersect(pos, buffer_view_rect);
        bool at_resize_handle = rect_p_intersect(pos, resize_handle_rect);
        if (at_buffer_view_rect || at_resize_handle)
        {
            return state->buffer_views[i];
        }
    }
    return NULL;
}

Cursor_Pos buffer_pos_to_cursor_pos(Vec_2 buffer_pos, Text_Buffer text_buffer, const Render_State *render_state)
{
    Cursor_Pos cursor;
    cursor.line = buffer_pos.y / (float)get_font_line_height(render_state->font);
    if (cursor.line < 0) {
        cursor.line = 0;
    } else if (cursor.line >= text_buffer.line_count) {
        cursor.line = text_buffer.line_count - 1;
    }
    cursor.col = get_char_i_at_pos_in_string(text_buffer.lines[cursor.line].str, render_state->font, buffer_pos.x);
    return cursor;
}

void viewport_snap_to_cursor(Text_Buffer text_buffer, Display_Cursor cursor, Viewport *viewport, Render_State *render_state)
{
    Rect viewport_r = viewport->rect;
    Rect_Bounds viewport_b = get_viewport_cursor_bounds(*viewport, render_state->font);
    Rect_Bounds cursor_b = rect_get_bounds(get_cursor_rect(text_buffer, cursor, render_state));
    float font_space_width = get_char_width(' ', render_state->font);
    float font_line_height = get_font_line_height(render_state->font);
    if (cursor_b.max_y <= viewport_b.min_y || cursor_b.min_y >= viewport_b.max_y)
    {
        if (cursor_b.max_y <= viewport_b.min_y)
        {
            viewport->rect.y = cursor_b.min_y - VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
            if (viewport->rect.y < 0.0f) viewport->rect.y = 0.0f;
        }
        else
        {
            viewport->rect.y = cursor_b.max_y - viewport_r.h + VIEWPORT_CURSOR_BOUNDARY_LINES * font_line_height;
            float buffer_max_y = (text_buffer.line_count - 1) * font_line_height;
            if (viewport->rect.y > buffer_max_y) viewport->rect.y = buffer_max_y;
        }
    }
    if (cursor_b.max_x <= viewport_b.min_x || cursor_b.min_x >= viewport_b.max_x)
    {
        if (cursor_b.max_x <= viewport_b.min_x)
        {
            viewport->rect.x = cursor_b.min_x - VIEWPORT_CURSOR_BOUNDARY_COLUMNS * font_space_width;
            if (viewport->rect.x < 0.0f) viewport->rect.x = 0.0f;
        }
        else
        {
            viewport->rect.x = cursor_b.max_x - viewport_r.w + VIEWPORT_CURSOR_BOUNDARY_COLUMNS * font_space_width;
            float buffer_max_x = 256 * font_space_width; // TODO: Determine max x coordinates based on longest line
            if (viewport->rect.x > buffer_max_x) viewport->rect.x = buffer_max_x;
        }
    }
}

bool is_cursor_pos_valid(Text_Buffer text_buf, Cursor_Pos buf_pos)
{
    bool is_valid = buf_pos.line >= 0 && buf_pos.line < text_buf.line_count &&
        buf_pos.col >= 0 && buf_pos.col < text_buf.lines[buf_pos.line].len;
    return is_valid;
}

bool is_cursor_pos_equal(Cursor_Pos a, Cursor_Pos b)
{
    return a.line == b.line && a.col == b.col;
}

Cursor_Pos cursor_pos_advance_char(Text_Buffer text_buffer, Cursor_Pos pos, int dir, bool can_switch_lines)
{
    int line = pos.line;
    int col = pos.col;
    if (dir > 0)
    {
        col++;
        if (col > text_buffer.lines[line].len - 1)
        {
            if (can_switch_lines && line < text_buffer.line_count - 1)
            {
                line++;
                col = 0;
            }
            else
            {
                col = text_buffer.lines[line].len - 1;
            }
        }
    }
    else
    {
        col--;
        if (col < 0)
        {
            if (can_switch_lines && line > 0)
            {
                line--;
                col = text_buffer.lines[line].len - 1;
            }
            else
            {
                col = 0;
            }
        }
    }
    return (Cursor_Pos){line, col};
}

Cursor_Pos cursor_pos_advance_line(Text_Buffer text_buffer, Cursor_Pos pos, int dir)
{
    int line = pos.line;
    int col = pos.col;
    if (dir > 0)
    {
        if (line < text_buffer.line_count - 1)
        {
            line++;
            if (col > text_buffer.lines[line].len - 1)
            {
                col = text_buffer.lines[line].len - 1;
            }
        }
        else
        {
            col = text_buffer.lines[line].len - 1;
        }
    }
    else
    {
        if (line > 0)
        {
            line--;
            if (col > text_buffer.lines[line].len - 1)
            {
                col = text_buffer.lines[line].len - 1;
            }
        }
        else
        {
            col = 0;
        }
    }
    return (Cursor_Pos){line, col};
}

void move_cursor_to_line(Buffer_View *buffer_view, Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state, int to_line, bool snap_viewport)
{
    int orig_cursor_line = cursor->pos.line;
    cursor->pos.line = to_line;
    if (cursor->pos.line < 0)
    {
        cursor->pos.line = 0;
        cursor->pos.col = 0;
    }
    if (cursor->pos.line >= text_buffer->line_count)
    {
        cursor->pos.line = text_buffer->line_count - 1;
        cursor->pos.col = text_buffer->lines[cursor->pos.line].len - 1;
    }
    if (cursor->pos.line != orig_cursor_line)
    {
        if (cursor->pos.col >= text_buffer->lines[cursor->pos.line].len)
        {
            cursor->pos.col = text_buffer->lines[cursor->pos.line].len - 1;
        }
        cursor->blink_time = 0.0f;
        if (snap_viewport)
        {
            viewport_snap_to_cursor(*text_buffer, *cursor, &buffer_view->viewport, &state->render_state);
        }
    }
}

void move_cursor_to_col(Buffer_View *buffer_view, Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state, int to_col, bool snap_viewport, bool can_switch_line)
{
    int prev_cursor_char = cursor->pos.col;
    (void)state;
    cursor->pos.col = to_col;
    if (cursor->pos.col < 0)
    {
        if (cursor->pos.line > 0 && can_switch_line)
        {
            cursor->pos.line--;
            cursor->pos.col = text_buffer->lines[cursor->pos.line].len - 1;
        }
        else
        {
            cursor->pos.col = 0;
        }
    }
    if (cursor->pos.col >= text_buffer->lines[cursor->pos.line].len)
    {
        if (cursor->pos.line < text_buffer->line_count - 1 && can_switch_line)
        {
            cursor->pos.line++;
            cursor->pos.col = 0;
        }
        else
        {
            cursor->pos.col = text_buffer->lines[cursor->pos.line].len - 1;
        }
    }
    if (cursor->pos.col != prev_cursor_char)
    {
        cursor->blink_time = 0.0f;
        if (snap_viewport)
        {
            viewport_snap_to_cursor(*text_buffer, *cursor, &buffer_view->viewport, &state->render_state);
        }
    }
}

void move_cursor_to_next_end_of_word(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    Text_Line *current_line = &active_view->buffer->text_buffer.lines[active_view->cursor.pos.line];
    int end_of_word = -1;
    for (int i = active_view->cursor.pos.col + 1; i < current_line->len - 1; i++)
    {
        if (isalnum(current_line->str[i]) && (isspace(current_line->str[i + 1]) || ispunct(current_line->str[i + 1])))
        {
            end_of_word = i + 1;
            break;
        }
    }
    if (end_of_word == -1)
    {
        move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, active_view->cursor.pos.line + 1, true);
        move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, 0, true, false);
    }
    else
    {
        move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, end_of_word, true, false);
    }
}

void move_cursor_to_prev_start_of_word(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    Text_Line *current_line = &active_view->buffer->text_buffer.lines[active_view->cursor.pos.line];
    int start_of_word = -1;
    for (int i = active_view->cursor.pos.col - 1; i > 0; i--)
    {
        if (isalnum(current_line->str[i]) && (isspace(current_line->str[i - 1]) || ispunct(current_line->str[i - 1])))
        {
            start_of_word = i;
            break;
        }
    }
    if (start_of_word == -1)
    {
        move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, active_view->cursor.pos.line - 1, true);
        move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, MAX_CHARS_PER_LINE, true, false);
    }
    else
    {
        move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, start_of_word, true, false);
    }
}

void move_cursor_to_next_white_line(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    for (int i = active_view->cursor.pos.line + 1; i < active_view->buffer->text_buffer.line_count; i++)
    {
        if (is_white_line(active_view->buffer->text_buffer.lines[i]))
        {
            move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, i, true);
            move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, 0, true, false);
            return;
        }
    }
    move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, MAX_LINES, true);
    move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, MAX_CHARS_PER_LINE, true, false);
}

void move_cursor_to_prev_white_line(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    for (int i = active_view->cursor.pos.line - 1; i >= 0; i--)
    {
        if (is_white_line(active_view->buffer->text_buffer.lines[i]))
        {
            move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, i, true);
            move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, 0, true, false);
            return;
        }
    }
    move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, 0, true);
    move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, 0, true, false);
}

void display_cursor_move(Buffer_View *buffer_view, Cursor_Pos cursor, Editor_State *state)
{
    move_cursor_to_line(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, cursor.line, true);
    move_cursor_to_col(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, cursor.col, true, false);
}

Text_Line make_text_line_dup(const char *line)
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

void insert_char(Text_Buffer *text_buffer, char c, Display_Cursor *cursor, Editor_State *state, bool auto_indent)
{
    Buffer_View *active_view = state->active_buffer_view;
    if (c == '\n') {
        Text_Line new_line = make_text_line_dup(text_buffer->lines[cursor->pos.line].str + cursor->pos.col);
        insert_line(text_buffer, new_line, cursor->pos.line + 1);
        resize_text_line(&text_buffer->lines[cursor->pos.line], cursor->pos.col + 1);
        text_buffer->lines[cursor->pos.line].str[cursor->pos.col] = '\n';
        move_cursor_to_line(active_view, text_buffer, cursor, state, cursor->pos.line + 1, true);
        move_cursor_to_col(active_view, text_buffer, cursor, state, 0, true, false);
        if (auto_indent)
        {
            int indent_spaces = get_line_indent(text_buffer->lines[cursor->pos.line - 1]);
            for (int i = 0; i < indent_spaces; i++)
            {
                insert_char(&active_view->buffer->text_buffer, ' ', &active_view->cursor, state, false);
            }
        }

    } else {
        resize_text_line(&text_buffer->lines[cursor->pos.line], text_buffer->lines[cursor->pos.line].len + 1);
        Text_Line *line = &text_buffer->lines[cursor->pos.line];
        for (int i = line->len - 1; i >= cursor->pos.col; i--) {
            line->str[i + 1] = line->str[i];
        }
        line->str[cursor->pos.col] = c;
        move_cursor_to_col(active_view, text_buffer, cursor, state, cursor->pos.col + 1, true, false);
    }
    if (active_view->buffer->kind == BUFFER_FILE)
    {
        active_view->buffer->file.info.has_been_modified = true;
    }
}

void remove_char(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    if (cursor->pos.col <= 0) {
        if (cursor->pos.line <= 0) {
            return;
        }
        Text_Line *line0 = &text_buffer->lines[cursor->pos.line - 1];
        Text_Line *line1 = &text_buffer->lines[cursor->pos.line];
        int line0_old_len = line0->len;
        resize_text_line(line0, line0_old_len - 1 + line1->len);
        strcpy(line0->str + line0_old_len - 1, line1->str);
        remove_line(text_buffer, cursor->pos.line);
        move_cursor_to_line(active_view, text_buffer, cursor, state, cursor->pos.line - 1, true);
        move_cursor_to_col(active_view, text_buffer, cursor, state, line0_old_len - 1, true, false);
    } else {
        Text_Line *line = &text_buffer->lines[cursor->pos.line];
        for (int i = cursor->pos.col; i <= line->len; i++) {
            line->str[i - 1] = line->str[i];
        }
        resize_text_line(&text_buffer->lines[cursor->pos.line], line->len - 1);
        move_cursor_to_col(active_view, text_buffer, cursor, state, cursor->pos.col - 1, true, false);
    }
    if (active_view->buffer->kind == BUFFER_FILE)
    {
        active_view->buffer->file.info.has_been_modified = true;
    }
}

void insert_indent(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state)
{
    if (is_selection_valid(*text_buffer, state->active_buffer_view->selection))
    {
        delete_selected(state);
    }
    int spaces_to_insert = INDENT_SPACES - cursor->pos.col % INDENT_SPACES;
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

void decrease_indent_level_line(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state)
{
    int indent = get_line_indent(text_buffer->lines[cursor->pos.line]);
    int chars_to_remove = indent % INDENT_SPACES;
    if (indent >= INDENT_SPACES && chars_to_remove == 0)
    {
        chars_to_remove = 4;
    }
    move_cursor_to_col(state->active_buffer_view, text_buffer, cursor, state, indent, false, false);
    for (int i = 0; i < chars_to_remove; i++)
    {
        remove_char(text_buffer, cursor, state);
    }
}

void decrease_indent_level(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    if (is_selection_valid(*text_buffer, active_view->selection))
    {
        Cursor_Pos start = active_view->selection.start;
        Cursor_Pos end = active_view->selection.end;
        if (start.line > end.line)
        {
            Cursor_Pos temp = start;
            start = end;
            end = temp;
        }
        for (int i = start.line; i <= end.line; i++)
        {
            move_cursor_to_line(active_view, text_buffer, cursor, state, i, false);
            decrease_indent_level_line(text_buffer, cursor, state);
        }
        move_cursor_to_line(active_view, text_buffer, cursor, state, start.line, false);
        int indent = get_line_indent(text_buffer->lines[cursor->pos.line]);
        move_cursor_to_col(active_view, text_buffer, cursor, state, indent, false, false);
    }
    else
    {
        decrease_indent_level_line(text_buffer, cursor, state);
    }
}

void increase_indent_level_line(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state)
{
    int indent = get_line_indent(text_buffer->lines[cursor->pos.line]);
    int chars_to_add = INDENT_SPACES - (indent % INDENT_SPACES);
    move_cursor_to_col(state->active_buffer_view, text_buffer, cursor, state, indent, false, false);
    for (int i = 0; i < chars_to_add; i++)
    {
        insert_char(text_buffer, ' ', cursor, state, false);
    }
}

void increase_indent_level(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    if (is_selection_valid(*text_buffer, active_view->selection))
    {
        Cursor_Pos start = active_view->selection.start;
        Cursor_Pos end = active_view->selection.end;
        if (start.line > end.line)
        {
            Cursor_Pos temp = start;
            start = end;
            end = temp;
        }
        for (int i = start.line; i <= end.line; i++)
        {
            move_cursor_to_line(active_view, text_buffer, cursor, state, i, false);
            increase_indent_level_line(text_buffer, cursor, state);
        }
        move_cursor_to_line(active_view, text_buffer, cursor, state, start.line, false);
        int indent = get_line_indent(text_buffer->lines[cursor->pos.line]);
        move_cursor_to_col(active_view, text_buffer, cursor, state, indent, false, false);
    }
    else
    {
        increase_indent_level_line(text_buffer, cursor, state);
    }
}

void delete_current_line(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    remove_line(&active_view->buffer->text_buffer, active_view->cursor.pos.line);
    move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, active_view->cursor.pos.line, false);
    move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, 0, false, false);
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

File_Info file_read_into_text_buffer(const char *path, Text_Buffer *text_buffer)
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

void file_write(Text_Buffer text_buffer, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) fatal("Failed to open file for writing at %s", path);
    for (int i = 0; i < text_buffer.line_count; i++) {
        fputs(text_buffer.lines[i].str, f);
    }
    fclose(f);
    trace_log("Saved file to %s", path);
}

void text_buffer_destroy(Text_Buffer *text_buffer)
{
    for (int i = 0; i < text_buffer->line_count; i++)
    {
        free(text_buffer->lines[i].str);
    }
    free(text_buffer->lines);
    text_buffer->lines = NULL;
    text_buffer->line_count = 0;
}

void text_buffer_validate(Text_Buffer *text_buffer)
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
    Buffer_View *active_view = state->active_buffer_view;
    active_view->selection.start = active_view->cursor.pos;
    active_view->selection.end = active_view->selection.start;
}

void extend_selection_to_cursor(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    active_view->selection.end = active_view->cursor.pos;
}

bool is_selection_valid(Text_Buffer text_buffer, Text_Selection selection)
{
    bool is_valid = is_cursor_pos_valid(text_buffer, selection.start) &&
        is_cursor_pos_valid(text_buffer, selection.end) &&
        !is_cursor_pos_equal(selection.start, selection.end);
    return is_valid;
}

void cancel_selection(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    active_view->selection.start = (Cursor_Pos){ -1, -1 };
    active_view->selection.end = (Cursor_Pos){ -1, -1 };
}

int selection_char_count(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    int char_count = 0;
    if (is_selection_valid(active_view->buffer->text_buffer, active_view->selection))
    {
        Cursor_Pos start = active_view->selection.start;
        Cursor_Pos end = active_view->selection.end;
        if (start.line > end.line)
        {
            Cursor_Pos temp = start;
            start = end;
            end = temp;
        }
        for (int i = start.line; i <= end.line; i++)
        {
            if (i == start.line && i == end.line)
            {
                int diff = end.col - start.col;
                if (diff < 0) diff = -diff;
                char_count += diff;
            }
            else if (i == start.line)
            {
                char_count += active_view->buffer->text_buffer.lines[i].len - start.col;
            }
            else if (i == end.line)
            {
                char_count += end.col;
            }
            else
            {
                char_count += active_view->buffer->text_buffer.lines[i].len;
            }
        }
    }
    return char_count;
}

void delete_selected(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    int char_count = selection_char_count(state);
    if (char_count > 0)
    {
        Cursor_Pos start = active_view->selection.start;
        Cursor_Pos end = active_view->selection.end;
        if (start.line > end.line || (start.line == end.line && start.col > end.col))
        {
            Cursor_Pos temp = start;
            start = end;
            end = temp;
        }
        move_cursor_to_line(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, end.line, false);
        move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, end.col, false, false);
        for (int i = 0; i < char_count; i++)
        {
            remove_char(&active_view->buffer->text_buffer, &active_view->cursor, state);
        }
        cancel_selection(state);
    }
}

void copy_at_selection(Editor_State *state)
{
    Buffer_View *active_view = state->active_buffer_view;
    if (is_selection_valid(active_view->buffer->text_buffer, active_view->selection))
    {
        Cursor_Pos start = active_view->selection.start;
        Cursor_Pos end = active_view->selection.end;
        if (start.line > end.line)
        {
            Cursor_Pos temp = start;
            start = end;
            end = temp;
        }
        state->copy_buffer.line_count = end.line - start.line + 1;
        state->copy_buffer.lines = xrealloc(state->copy_buffer.lines, state->copy_buffer.line_count * sizeof(state->copy_buffer.lines[0]));
        int copy_buffer_i = 0;
        for (int i = start.line; i <= end.line; i++)
        {
            int start_c, end_c;
            if (i == start.line && i == end.line)
            {
                start_c = start.col;
                end_c = end.col;
                if (start_c > end_c)
                {
                    int temp = start_c;
                    start_c = end_c;
                    end_c = temp;
                }
            }
            else if (i == start.line)
            {
                start_c = start.col;
                end_c = -1;
            }
            else if (i == end.line)
            {
                start_c = 0;
                end_c = end.col;
            }
            else
            {
                start_c = 0;
                end_c = -1;
            }
            state->copy_buffer.lines[copy_buffer_i++] = copy_text_line(active_view->buffer->text_buffer.lines[i], start_c, end_c);
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
    Buffer_View *active_view = state->active_buffer_view;
    if (state->copy_buffer.line_count > 0)
    {
        if (is_selection_valid(active_view->buffer->text_buffer, active_view->selection))
        {
            delete_selected(state);
        }
        for (int i = 0; i < state->copy_buffer.line_count; i++)
        {
            for (int char_i = 0; char_i < state->copy_buffer.lines[i].len; char_i++)
            {
                insert_char(&active_view->buffer->text_buffer, state->copy_buffer.lines[i].str[char_i], &active_view->cursor, state, false);
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

void handle_key_input(GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    (void) window;
    Buffer_View *active_view = state->active_buffer_view;
    switch(key)
    {
        // case GLFW_KEY_ESCAPE: if (action == GLFW_PRESS)
        // {
        //     glfwSetWindowShouldClose(window, 1);
        // } break;
        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
        if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            Cursor_Movement_Dir dir = get_cursor_movement_dir_by_key(key);
            handle_cursor_movement_keys(active_view, dir, mods & GLFW_MOD_SHIFT, mods & GLFW_MOD_ALT, mods & GLFW_MOD_SUPER, state);
        } break;
        case GLFW_KEY_ENTER: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            switch (active_view->buffer->kind)
            {
                case BUFFER_PROMPT:
                {
                    Prompt_Result prompt_result = prompt_parse_result(active_view->buffer->text_buffer);
                    prompt_submit(active_view->buffer->prompt.context, prompt_result, active_view->outer_rect, window, state);
                    buffer_view_destroy(active_view, state);
                } break;

                default:
                {
                    insert_char(&active_view->buffer->text_buffer, '\n', &active_view->cursor, state, true);
                } break;
            }
        } break;
        case GLFW_KEY_BACKSPACE: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            if (is_selection_valid(active_view->buffer->text_buffer, active_view->selection)) delete_selected(state);
            else remove_char(&active_view->buffer->text_buffer, &active_view->cursor, state);
        } break;
        case GLFW_KEY_TAB: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            if (mods == GLFW_MOD_ALT)
            {
                int indent_i = get_line_indent(active_view->buffer->text_buffer.lines[active_view->cursor.pos.line]);
                move_cursor_to_col(active_view, &active_view->buffer->text_buffer, &active_view->cursor, state, indent_i, true, false);
            }
            else insert_indent(&active_view->buffer->text_buffer, &active_view->cursor, state);
        } break;
        case GLFW_KEY_LEFT_BRACKET: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            decrease_indent_level(&active_view->buffer->text_buffer, &active_view->cursor, state);
        } break;
        case GLFW_KEY_RIGHT_BRACKET: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            increase_indent_level(&active_view->buffer->text_buffer, &active_view->cursor, state);
        } break;
        case GLFW_KEY_C: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            copy_at_selection(state);
        } break;
        case GLFW_KEY_V: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            paste_from_copy_buffer(state);
        } break;
        case GLFW_KEY_X: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            delete_current_line(state);
        } break;
        case GLFW_KEY_F12: if (action == GLFW_PRESS)
        {
            state->should_break = true;
        } break;
        case GLFW_KEY_F9: if (action == GLFW_PRESS)
        {
            rebuild_dl();
        } break;
        case GLFW_KEY_F8: if (action == GLFW_PRESS)
        {
            text_buffer_validate(&active_view->buffer->text_buffer);
            trace_log("Validated text buffer");
        } break;
        case GLFW_KEY_F7: if (action == GLFW_PRESS)
        {
            // TODO: Implement reloading current file
        } break;
        case GLFW_KEY_S: if (mods & GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (mods & GLFW_MOD_SHIFT)
            {
                if (active_view != NULL && active_view->buffer->kind == BUFFER_FILE)
                {
                    Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
                    buffer_view_prompt(
                        "Save as:",
                        prompt_create_context_save_as(active_view),
                        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                        state);
                }
            }
            else
            {
                file_write(active_view->buffer->text_buffer, active_view->buffer->file.info.path);
                if (active_view->buffer->kind == BUFFER_FILE)
                {
                    active_view->buffer->file.info.has_been_modified = false;
                }
            }
        } break;
        case GLFW_KEY_EQUAL: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            active_view->viewport.zoom += 0.25f;
        } break;
        case GLFW_KEY_MINUS: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            active_view->viewport.zoom -= 0.25f;
        } break;
        case GLFW_KEY_W: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (state->active_buffer_view)
            {
                buffer_view_destroy(state->active_buffer_view, state);
            }
        } break;
        case GLFW_KEY_1: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
            Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
            buffer_view_open_file(
                FILE_PATH1,
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
                state);
        } break;
        case GLFW_KEY_O: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            buffer_view_prompt(
                "Open file:",
                prompt_create_context_open_file(),
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                state);
        } break;
        case GLFW_KEY_G: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (active_view != NULL && active_view->buffer->kind == BUFFER_FILE)
            {
                Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
                buffer_view_prompt(
                    "Go to line:",
                    prompt_create_context_go_to_line(active_view),
                    (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                    state);
            }
        } break;
    }
}

void handle_char_input(Editor_State *state, char c)
{
    Buffer_View *active_view = state->active_buffer_view;
    if (active_view)
    {
        if (is_selection_valid(active_view->buffer->text_buffer, active_view->selection))
        {
            delete_selected(state);
        }
        insert_char(&active_view->buffer->text_buffer, c, &active_view->cursor, state, false);
    }
}

void handle_mouse_input(GLFWwindow *window, Editor_State *state)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
        Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
        bool is_just_pressed = !state->left_mouse_down;
        state->left_mouse_down = true;
        if (is_just_pressed)
        {
            // TODO: Mouse screen position will have to be converted to canvas position
            Buffer_View *clicked_view = get_buffer_view_at_pos(mouse_canvas_pos, state);
            if (clicked_view != NULL)
            {
                if (state->active_buffer_view != clicked_view)
                {
                    buffer_view_set_active(clicked_view, state);
                    state->left_mouse_handled = true;
                }
                Rect text_area_rect = buffer_view_get_text_area_rect(*clicked_view, &state->render_state);
                Rect resize_handle_rect = buffer_view_get_resize_handle_rect(*clicked_view, &state->render_state);
                if (rect_p_intersect(mouse_canvas_pos, resize_handle_rect))
                    state->is_buffer_view_resize = true;
                else if (rect_p_intersect(mouse_canvas_pos, text_area_rect))
                    state->is_buffer_view_text_area_click = true;
                else
                    state->is_buffer_view_drag = true;
            }
        }

        Buffer_View *active_view = state->active_buffer_view;
        if (!state->left_mouse_handled && state->is_buffer_view_text_area_click)
        {
            bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            Vec_2 mouse_text_area_pos = buffer_view_canvas_pos_to_text_area_pos(*active_view, mouse_canvas_pos, &state->render_state);
            handle_mouse_text_area_click(active_view, is_shift_pressed, is_just_pressed, mouse_text_area_pos, state);
        }
        else if (state->is_buffer_view_drag)
        {
            Vec_2 mouse_delta = get_mouse_delta(window, state);
            Rect new_rect = active_view->outer_rect;
            new_rect.x += mouse_delta.x;
            new_rect.y += mouse_delta.y;
            buffer_view_set_rect(state->active_buffer_view, new_rect, &state->render_state);
        }
        else if (state->is_buffer_view_resize)
        {
            Vec_2 mouse_delta = get_mouse_delta(window, state);
            Rect new_rect = active_view->outer_rect;
            new_rect.w += mouse_delta.x;
            new_rect.h += mouse_delta.y;
            buffer_view_set_rect(state->active_buffer_view, new_rect, &state->render_state);
        }
    }
    else
    {
        state->left_mouse_down = false;
        state->left_mouse_handled = false;
        state->is_buffer_view_text_area_click = false;
        state->is_buffer_view_drag = false;
        state->is_buffer_view_resize = false;
    }


}

Cursor_Movement_Dir get_cursor_movement_dir_by_key(int key)
{
    switch (key)
    {
        case GLFW_KEY_LEFT: return CURSOR_MOVE_LEFT;
        case GLFW_KEY_RIGHT: return CURSOR_MOVE_RIGHT;
        case GLFW_KEY_UP: return CURSOR_MOVE_UP;
        case GLFW_KEY_DOWN: return CURSOR_MOVE_DOWN;
        default: return CURSOR_MOVE_UP;
    }
}

void handle_cursor_movement_keys(Buffer_View *buffer_view, Cursor_Movement_Dir dir, bool with_selection, bool big_steps, bool start_end, Editor_State *state)
{
    if (with_selection && !is_selection_valid(buffer_view->buffer->text_buffer, buffer_view->selection)) start_selection_at_cursor(state);

    switch (dir)
    {
        case CURSOR_MOVE_LEFT:
        {
            if (big_steps) move_cursor_to_prev_start_of_word(state);
            else if (start_end) move_cursor_to_col(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, 0, true, false);
            else
            {
                buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1, true);
                viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor, &buffer_view->viewport, &state->render_state);
                buffer_view->cursor.blink_time = 0.0f;
            }
        } break;
        case CURSOR_MOVE_RIGHT:
        {
            if (big_steps) move_cursor_to_next_end_of_word(state);
            else if (start_end) move_cursor_to_col(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, MAX_CHARS_PER_LINE, true, false);
            else
            {
                buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1, true);
                viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor, &buffer_view->viewport, &state->render_state);
                buffer_view->cursor.blink_time = 0.0f;
            }
        } break;
        case CURSOR_MOVE_UP:
        {
            if (big_steps) move_cursor_to_prev_white_line(state);
            else if (start_end)
            {
                move_cursor_to_line(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, 0, true);
                move_cursor_to_col(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, 0, true, false);
            }
            else
            {
                buffer_view->cursor.pos = cursor_pos_advance_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1);
                viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor, &buffer_view->viewport, &state->render_state);
                buffer_view->cursor.blink_time = 0.0f;
            }
        } break;
        case CURSOR_MOVE_DOWN:
        {
            if (big_steps) move_cursor_to_next_white_line(state);
            else if (start_end)
            {
                move_cursor_to_line(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, MAX_LINES, true);
                move_cursor_to_col(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, MAX_CHARS_PER_LINE, true, false);
            }
            else
            {
                buffer_view->cursor.pos = cursor_pos_advance_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1);
                viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor, &buffer_view->viewport, &state->render_state);
                buffer_view->cursor.blink_time = 0.0f;
            }
        } break;
    }

    if (with_selection) extend_selection_to_cursor(state);
    else cancel_selection(state);
}

void handle_mouse_text_area_click(Buffer_View *buffer_view, bool with_selection, bool just_pressed, Vec_2 mouse_text_area_pos, Editor_State *state)
{
    if (with_selection && just_pressed && !is_selection_valid(buffer_view->buffer->text_buffer, buffer_view->selection))
    {
        start_selection_at_cursor(state);
    }
    Vec_2 mouse_buffer_pos = buffer_view_text_area_pos_to_buffer_pos(*buffer_view, mouse_text_area_pos);
    Cursor_Pos mouse_cursor = buffer_pos_to_cursor_pos(mouse_buffer_pos, buffer_view->buffer->text_buffer, &state->render_state);
    move_cursor_to_line(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, mouse_cursor.line, false);
    move_cursor_to_col(buffer_view, &buffer_view->buffer->text_buffer, &buffer_view->cursor, state, mouse_cursor.col, false, false);
    extend_selection_to_cursor(state);
    if (!with_selection && just_pressed)
    {
        start_selection_at_cursor(state);
    }
    if (with_selection || !just_pressed)
    {
        extend_selection_to_cursor(state);
    }
}
