#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include <stb_image.h>
#include <stb_truetype.h>

#include "editor.h"

#include "shaders.h"
#include "util.h"

#include "unit_tests.c"

void _init(GLFWwindow *window, void *_state)
{
    (void)window; Editor_State *state = _state; (void)state;
    bassert(sizeof(*state) < 4096);
    glfwSetWindowUserPointer(window, _state);

    initialize_render_state(window, &state->render_state);

    state->canvas_viewport.zoom = 1.0f;
    viewport_set_outer_rect(&state->canvas_viewport, (Rect){0, 0, state->render_state.window_dim.x, state->render_state.window_dim.y});

    state->working_dir = sys_get_working_dir();
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

    glBindVertexArray(state->render_state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->render_state.vbo);

    glUseProgram(state->render_state.grid_shader);
    draw_grid(state->canvas_viewport, &state->render_state);

    glUseProgram(state->render_state.main_shader);

    // Render frames backwards for correct z ordering
    for (int i = state->view_count - 1; i >= 0; i--)
    {
        Frame *frame = state->frames[i];
        bool is_active = frame == state->active_frame;
        draw_frame(*frame, is_active, state->canvas_viewport, &state->render_state, state->delta_time);
    }

    draw_status_bar(window, state, &state->render_state);

    handle_mouse_input(window, state);

    state->mouse_state.prev_mouse_pos = get_mouse_screen_pos(window);

    if (state->mouse_state.scroll_timeout > 0.0f)
    {
        state->mouse_state.scroll_timeout -= state->delta_time;
    }
    else
    {
        state->mouse_state.scrolled_frame = NULL;
    }

    glfwSwapBuffers(window);
    glfwPollEvents();

    state->frame_count++;
    state->fps_frame_count++;
}

void char_callback(GLFWwindow *window, unsigned int codepoint)
{
    (void)window; Editor_State *state = glfwGetWindowUserPointer(window);
    if (state->active_frame != NULL && state->active_frame->view->kind == VIEW_KIND_BUFFER)
    {
        buffer_view_handle_char_input(&state->active_frame->view->bv, (char)codepoint, &state->render_state);
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
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            handle_mouse_click(window, state);
        }
        else if (action == GLFW_RELEASE)
        {
            handle_mouse_release(&state->mouse_state);
        }
    }
}

void scroll_callback(GLFWwindow *window, double x_offset, double y_offset)
{
    (void)window; (void)x_offset; (void)y_offset; Editor_State *state = glfwGetWindowUserPointer(window); (void)state;
    if (state->mouse_state.scroll_timeout <= 0.0f)
    {
        Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
        Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
        state->mouse_state.scrolled_frame = frame_at_pos(mouse_canvas_pos, state);
    }
    state->mouse_state.scroll_timeout = 0.1f;
    bool scroll_handled_by_frame = false;
    if (state->mouse_state.scrolled_frame)
    {
        scroll_handled_by_frame = frame_handle_scroll(state->mouse_state.scrolled_frame, x_offset, y_offset, &state->render_state);
    }
    if (!scroll_handled_by_frame)
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

Framebuffer gl_create_framebuffer(int w, int h)
{
    Framebuffer framebuffer;
    framebuffer.w = w;
    framebuffer.h = h;

    glGenFramebuffers(1, &framebuffer.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.fbo);

    glGenTextures(1, &framebuffer.tex);
    glBindTexture(GL_TEXTURE_2D, framebuffer.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, framebuffer.w, framebuffer.h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.tex, 0);

    glGenRenderbuffers(1, &framebuffer.depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, framebuffer.depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, framebuffer.depth_rb);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        log_warning("gl_create_framebuffer: Failed to create framebuffer");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return framebuffer;
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
    render_state->image_shader = gl_create_shader_program(shader_main_vert_src, shader_image_frag_src);
    render_state->framebuffer_shader = gl_create_shader_program(shader_framebuffer_vert_src, shader_framebuffer_frag_src);

    render_state->main_shader_mvp_loc = glGetUniformLocation(render_state->main_shader, "u_mvp");
    render_state->grid_shader_mvp_loc = glGetUniformLocation(render_state->grid_shader, "u_mvp");
    render_state->grid_shader_offset_loc = glGetUniformLocation(render_state->grid_shader, "u_offset");
    render_state->grid_shader_resolution_loc = glGetUniformLocation(render_state->grid_shader, "u_resolution");
    render_state->image_shader_mvp_loc = glGetUniformLocation(render_state->image_shader, "u_mvp");
    render_state->framebuffer_shader_mvp_loc = glGetUniformLocation(render_state->framebuffer_shader, "u_mvp");

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

    glGenTextures(1, &render_state->white_texture);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
    unsigned char white_texture_bytes[] = { 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, white_texture_bytes);

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

Buffer *buffer_create_generic(Text_Buffer text_buffer, Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);
    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->kind = BUFFER_KIND_GENERIC;
    buffer->text_buffer = text_buffer;
    *new_slot = buffer;
    return *new_slot;
}

Buffer *buffer_create_read_file(const char *path, Editor_State *state)
{
    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->kind = BUFFER_KIND_FILE;
    bool opened = file_read_into_text_buffer(path, &buffer->text_buffer, &buffer->file.info);
    if (!opened)
    {
        free(buffer);
        return NULL;
    }
    Buffer **new_slot = buffer_create_new_slot(state);
    *new_slot = buffer;
    return *new_slot;
}

Buffer *buffer_create_empty_file(Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);

    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->kind = BUFFER_KIND_FILE;
    buffer->file.info.path = NULL;
    buffer->text_buffer = text_buffer_create_from_lines("", NULL);

    *new_slot = buffer;
    return *new_slot;
}

Buffer *buffer_create_prompt(const char *prompt_text, Prompt_Context context, Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);

    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->kind = BUFFER_KIND_PROMPT;

    buffer->text_buffer.line_count = 2;
    buffer->text_buffer.lines = xmalloc(buffer->text_buffer.line_count * sizeof(buffer->text_buffer.lines[0]));
    buffer->prompt.context = context;

    char prompt_line_buf[MAX_CHARS_PER_LINE];
    snprintf(prompt_line_buf, sizeof(prompt_line_buf), "%s\n", prompt_text);
    buffer->text_buffer.lines[0] = text_line_make_dup(prompt_text);
    buffer->text_buffer.lines[1] = text_line_make_dup("\n");

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
        case BUFFER_KIND_GENERIC:
        case BUFFER_KIND_FILE:
        case BUFFER_KIND_PROMPT:
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

int frame___get_index(Frame *frame, Editor_State *state)
{
    int index = 0;
    Frame **frame_c = state->frames;
    while (*frame_c != frame)
    {
        index++;
        frame_c++;
    }
    return index;
}

bool frame___exists(Frame *frame, Editor_State *state)
{
    for (int i = 0; i < state->frame_count_; i++)
    {
        if (state->frames[i] == frame)
            return true;
    }
    return false;
}

Frame **frame___create_new_slot(Editor_State *state)
{
    state->frame_count_++;
    state->frames = xrealloc(state->frames, state->frame_count_ * sizeof(state->frames[0]));
    return &state->frames[state->frame_count_ - 1];
}

void frame___free_slot(Frame *frame, Editor_State *state)
{
    int index_to_delete = frame___get_index(frame, state);
    for (int i = index_to_delete; i < state->frame_count_ - 1; i++)
    {
        state->frames[i] = state->frames[i + 1];
    }
    state->frame_count_--;
    state->frames = xrealloc(state->frames, state->frame_count * sizeof(state->frames[0]));
}

Frame *frame_create(View *view, Rect rect, Editor_State *state)
{
    Frame **new_slot = frame___create_new_slot(state);
    Frame *frame = xcalloc(sizeof(Frame));
    frame->view = view;
    frame_set_rect(frame, rect, &state->render_state);
    *new_slot = frame;
    return *new_slot;
}

void frame_destroy(Frame *frame, Editor_State *state)
{
    view_destroy(frame->view, state);
    frame___free_slot(frame, state);
    free(frame);
    if (state->active_frame == frame)
    {
        state->active_frame = (state->frame_count_ > 0) ? state->frames[0] : NULL;
    }
}

void frame_set_active(Frame *frame, Editor_State *state)
{
    int active_index = frame___get_index(frame, state);
    for (int i = active_index; i > 0; i--)
    {
        state->frames[i] = state->frames[i - 1];
    }
    state->frames[0] = frame;
    state->active_frame = frame;
}

Rect frame_get_resize_handle_rect(Frame frame, const Render_State *render_state)
{
    Rect r;
    r.x = frame.outer_rect.x + frame.outer_rect.w - render_state->buffer_view_resize_handle_radius;
    r.y = frame.outer_rect.y + frame.outer_rect.h - render_state->buffer_view_resize_handle_radius;
    r.w = 2 * render_state->buffer_view_resize_handle_radius;
    r.h = 2 * render_state->buffer_view_resize_handle_radius;
    return r;
}

void frame_set_rect(Frame *frame, Rect rect, const Render_State *render_state)
{
    frame->outer_rect = rect;
    view_set_rect(frame->view, rect, render_state);
}

Frame *frame_at_pos(Vec_2 pos, Editor_State *state)
{
    for (int i = 0; i < state->frame_count_; i++)
    {
        Rect buffer_view_rect = state->frames[i]->outer_rect;
        Rect resize_handle_rect = frame_get_resize_handle_rect(*state->frames[i], &state->render_state);
        bool at_buffer_view_rect = rect_p_intersect(pos, buffer_view_rect);
        bool at_resize_handle = rect_p_intersect(pos, resize_handle_rect);
        if (at_buffer_view_rect || at_resize_handle)
        {
            return state->frames[i];
        }
    }
    return NULL;
}

bool frame_handle_scroll(Frame *frame, float x_offset, float y_offset, const Render_State *render_state)
{
    bassert(frame != NULL);
    if (frame->view->kind == VIEW_KIND_BUFFER)
    {
        Buffer_View *buffer_view = &frame->view->bv;
        buffer_view->viewport.rect.x -= x_offset * SCROLL_SENS;
        buffer_view->viewport.rect.y -= y_offset * SCROLL_SENS;
        if (buffer_view->viewport.rect.x < 0.0f) buffer_view->viewport.rect.x = 0.0f;
        if (buffer_view->viewport.rect.y < 0.0f) buffer_view->viewport.rect.y = 0.0f;
        float buffer_max_y = (buffer_view->buffer->text_buffer.line_count - 1) * get_font_line_height(render_state->font);
        if (buffer_view->viewport.rect.y > buffer_max_y) buffer_view->viewport.rect.y = buffer_max_y;
        float buffer_max_x = 256 * get_font_line_height(render_state->font); // TODO: Determine max x coordinates based on longest line
        if (buffer_view->viewport.rect.x > buffer_max_x) buffer_view->viewport.rect.x = buffer_max_x;
        return true;
    }
    return false;
}

Frame *frame_create_buffer_view_generic(Text_Buffer text_buffer, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_generic(text_buffer, state);
    View *view = buffer_view_create(buffer, state);
    Frame *frame = frame_create(view, rect, state);
    frame_set_active(frame, state);
    return frame;
}

Frame *frame_create_buffer_view_open_file(const char *file_path, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_read_file(file_path, state);
    if (buffer != NULL)
    {
        View *view = buffer_view_create(buffer, state);
        Frame *frame = frame_create(view, rect, state);
        frame_set_active(frame, state);
        return frame;
    }
    return NULL;
}

Frame *frame_create_buffer_view_empty_file(Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_empty_file(state);
    View *view = buffer_view_create(buffer, state);
    Frame *frame = frame_create(view, rect, state);
    frame_set_active(frame, state);
    return frame;
}

Frame *frame_create_buffer_view_prompt(const char *prompt_text, Prompt_Context context, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_prompt(prompt_text, context, state);
    View *view = buffer_view_create(buffer, state);
    view->bv.cursor.pos = cursor_pos_clamp(view->bv.buffer->text_buffer, (Cursor_Pos){2, 0});
    Frame *frame = frame_create(view, rect, state);
    frame_set_active(frame, state);
    return frame;
}

Frame *frame_create_image_view(const char *file_path, Rect rect, Editor_State *state)
{
    Image image = file_open_image(file_path);
    View *view = image_view_create(image, state);
    Frame *frame = frame_create(view, rect, state);
    frame_set_active(frame, state);
    return frame;
}

Frame *frame_create_live_scene_view(const char *dylib_path, Rect rect, Editor_State *state)
{
    Framebuffer framebuffer = gl_create_framebuffer((int)rect.w, (int)rect.h);
    Live_Scene *live_scene = live_scene_create(dylib_path, rect.w, rect.h);
    View *view = live_scene_view_create(framebuffer, live_scene, state);
    Frame *frame = frame_create(view, rect, state);
    frame_set_active(frame, state);
    return frame;
}

int view___get_index(View *view, Editor_State *state)
{
    int index = 0;
    View **view_c = state->views;
    while (*view_c != view)
    {
        index++;
        view_c++;
    }
    return index;
}

View **view___create_new_slot(Editor_State *state)
{
    state->view_count++;
    state->views = xrealloc(state->views, state->view_count * sizeof(state->views[0]));
    return &state->views[state->view_count - 1];
}

void view___free_slot(View *view, Editor_State *state)
{
    int index_to_delete = view___get_index(view, state);
    for (int i = index_to_delete; i < state->view_count - 1; i++)
    {
        state->views[i] = state->views[i + 1];
    }
    state->view_count--;
    state->views = xrealloc(state->views, state->view_count * sizeof(state->views[0]));
}

View *view_create(Editor_State *state)
{
    View **new_slot = view___create_new_slot(state);
    View *view = xcalloc(sizeof(View));
    *new_slot = view;
    return *new_slot;
}

void view_destroy(View *view, Editor_State *state)
{
    switch(view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            buffer_destroy(view->bv.buffer, state);
            view___free_slot(view, state);
            free(view);
        } break;

        case VIEW_KIND_IMAGE:
        {
            image_destroy(view->iv.image);
            view___free_slot(view, state);
            free(view);
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            framebuffer_destroy(view->lsv.framebuffer);
            live_scene_destroy(view->lsv.live_scene);
            view___free_slot(view, state);
            free(view);
        } break;

        default:
        {
            log_warning("view_destroy: Unhandled View kind: %d", view->kind);
        } break;
    }
}

bool view_exists(View *view, Editor_State *state)
{
    for (int i = 0; i < state->view_count; i++)
    {
        if (state->views[i] == view)
            return true;
    }
    return false;
}

void view_set_rect(View *view, Rect rect, const Render_State *render_state)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            Rect text_area_rect = buffer_view_get_text_area_rect(view->bv, rect, render_state);
            viewport_set_outer_rect(&view->bv.viewport, text_area_rect);
        } break;

        case VIEW_KIND_IMAGE:
        {
            float x = rect.x + render_state->buffer_view_padding;
            float y = rect.y + render_state->buffer_view_padding;
            float w = rect.w - 2 * render_state->buffer_view_padding;
            float h = rect.h - 2 * render_state->buffer_view_padding;
            if (w > h)
            {
                view->iv.image_rect.y = y;
                view->iv.image_rect.h = h;
                view->iv.image_rect.w = view->iv.image_rect.h / view->iv.image.height * view->iv.image.width;
                view->iv.image_rect.x = x + w * 0.5f - view->iv.image_rect.w * 0.5f;
            }
            else
            {
                view->iv.image_rect.x = x;
                view->iv.image_rect.w = w;
                view->iv.image_rect.h = view->iv.image_rect.w / view->iv.image.width * view->iv.image.height;
                view->iv.image_rect.y = y + h * 0.5f - view->iv.image_rect.h * 0.5f;
            }
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            view->lsv.framebuffer_rect.x = rect.x + render_state->buffer_view_padding;
            view->lsv.framebuffer_rect.y = rect.y + render_state->buffer_view_padding;
            view->lsv.framebuffer_rect.w = rect.w - 2 * render_state->buffer_view_padding;
            view->lsv.framebuffer_rect.h = rect.h - 2 * render_state->buffer_view_padding;
        } break;

        default:
        {
            log_warning("view_set_rect: Unhandled View kind: %d", view->kind);
        } break;
    }
}

View *buffer_view_create(Buffer *buffer, Editor_State *state)
{
    View *view = view_create(state);
    view->kind = VIEW_KIND_BUFFER;
    view->bv.viewport.zoom = DEFAULT_ZOOM;
    view->bv.buffer = buffer;
    return view;
}

Rect buffer_view_get_text_area_rect(Buffer_View buffer_view, Rect frame_rect, const Render_State *render_state)
{
    (void)buffer_view;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect r;
    r.x = frame_rect.x + pad + line_num_w + pad;
    r.y = frame_rect.y + pad + name_h + pad;
    r.w = frame_rect.w - pad - line_num_w - pad - pad;
    r.h = frame_rect.h - pad - name_h - pad - pad;
    return r;
}

Rect buffer_view_get_line_num_col_rect(Buffer_View buffer_view, Rect frame_rect, const Render_State *render_state)
{
    (void)buffer_view;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect r;
    r.x = frame_rect.x + pad;
    r.y = frame_rect.y + pad + name_h + pad;
    r.w = line_num_w;
    r.h = frame_rect.h - pad - name_h - pad - pad;
    return r;
}

Rect buffer_view_get_name_rect(Buffer_View buffer_view, Rect frame_rect, const Render_State *render_state)
{
    (void)buffer_view;
    const float right_limit = 40.0f;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect r;
    r.x = frame_rect.x + pad + line_num_w + pad;
    r.y = frame_rect.y + pad;
    r.w = frame_rect.w - pad - line_num_w - pad - pad - right_limit;
    r.h = name_h;
    return r;
}

Vec_2 buffer_view_canvas_pos_to_text_area_pos(Buffer_View buffer_view, Rect frame_rect, Vec_2 canvas_pos, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, frame_rect, render_state);
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

void image_destroy(Image image)
{
    glDeleteTextures(1, &image.texture);
}

void framebuffer_destroy(Framebuffer framebuffer)
{
    glDeleteRenderbuffers(1, &framebuffer.depth_rb);
    glDeleteTextures(1, &framebuffer.tex);
    glDeleteFramebuffers(1, &framebuffer.fbo);
}

View *image_view_create(Image image, Editor_State *state)
{
    View *view = view_create(state);
    view->kind = VIEW_KIND_IMAGE;
    view->iv.image = image;
    return view;
}

Live_Scene *live_scene_create(const char *path, float w, float h)
{
    Live_Scene *live_scene = xcalloc(sizeof(Live_Scene));
    live_scene->state = xcalloc(4096);
    live_scene->dylib = live_scene_load_dylib(path);
    live_scene->dylib.init(live_scene->state, w, h);
    return live_scene;
}

void live_scene_check_hot_reload(Live_Scene *live_scene)
{
    time_t dl_current_timestamp = get_file_timestamp(live_scene->dylib.dl_path);
    if (dl_current_timestamp != live_scene->dylib.dl_timestamp)
    {
        dlclose(live_scene->dylib.dl_handle);
        live_scene->dylib= live_scene_load_dylib(live_scene->dylib.dl_path);
        live_scene->dylib.reload(live_scene->state);
        trace_log("live_scene_check_hot_reload: Reloaded live scene (%s)", live_scene->dylib.dl_path);
    }
}

void live_scene_destroy(Live_Scene *live_scene)
{
    live_scene->dylib.destroy(live_scene->state);
    free(live_scene->state);
    live_scene->state = NULL;
    free(live_scene);
}

View *live_scene_view_create(Framebuffer framebuffer, Live_Scene *live_scene, Editor_State *state)
{
    View *view = view_create(state);
    view->kind = VIEW_KIND_LIVE_SCENE;
    view->lsv.framebuffer = framebuffer;
    view->lsv.live_scene = live_scene;
    return view;
}

Prompt_Context prompt_create_context_open_file()
{
    Prompt_Context context;
    context.kind = PROMPT_OPEN_FILE;
    return context;
}

Prompt_Context prompt_create_context_go_to_line(Buffer_View *for_buffer_view)
{
    Prompt_Context context;
    context.kind = PROMPT_GO_TO_LINE;
    context.go_to_line.for_buffer_view = for_buffer_view;
    return context;
}

Prompt_Context prompt_create_context_search_next(Buffer_View *for_buffer_view)
{
    Prompt_Context context;
    context.kind = PROMPT_SEARCH_NEXT;
    context.go_to_line.for_buffer_view = for_buffer_view;
    return context;
}

Prompt_Context prompt_create_context_save_as(Buffer_View *for_buffer_view)
{
    Prompt_Context context;
    context.kind = PROMPT_SAVE_AS;
    context.go_to_line.for_buffer_view = for_buffer_view;
    return context;
}

Prompt_Context prompt_create_context_change_working_dir()
{
    Prompt_Context context;
    context.kind = PROMPT_CHANGE_WORKING_DIR;
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

bool prompt_submit(Prompt_Context context, Prompt_Result result, Rect prompt_rect, GLFWwindow *window, Editor_State *state)
{
    (void)window; (void)state;
    switch (context.kind)
    {
        case PROMPT_OPEN_FILE:
        {
            Rect new_frame_rect =
            {
                .x = prompt_rect.x,
                .y = prompt_rect.y,
                .w = 500,
                .h = 500
            };
            File_Kind file_kind = file_detect_kind(result.str);
            if (file_kind == FILE_KIND_IMAGE)
            {
                frame_create_image_view(result.str, new_frame_rect, state);
            }
            else if (file_kind == FILE_KIND_DYLIB)
            {
                frame_create_live_scene_view(result.str, new_frame_rect, state);
            }
            else
            {
                Frame *frame = frame_create_buffer_view_open_file(result.str, new_frame_rect, state);
                if (frame == NULL)
                {
                    return false;
                }
            }
        } break;

        case PROMPT_GO_TO_LINE:
        {
            Buffer_View *buffer_view = context.go_to_line.for_buffer_view;
            if (view_exists((View *)buffer_view, state))
            {
                int go_to_line = xstrtoint(result.str);
                buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, (Cursor_Pos){go_to_line - 1, 0});
                viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
                buffer_view->cursor.blink_time = 0.0f;
            }
            else log_warning("prompt_submit: PROMPT_GO_TO_LINE: Buffer_View %p does not exist", context.go_to_line.for_buffer_view);
        } break;

        case PROMPT_SEARCH_NEXT:
        {
            Buffer_View *buffer_view = context.go_to_line.for_buffer_view;
            if (view_exists((View *)buffer_view, state))
            {
                if (state->prev_search) free(state->prev_search);
                state->prev_search = xstrdup(result.str);

                Cursor_Pos found_pos;
                bool found = text_buffer_search_next(&buffer_view->buffer->text_buffer, result.str, buffer_view->cursor.pos, &found_pos);
                if (found)
                {
                    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, found_pos);
                    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
                    buffer_view->cursor.blink_time = 0.0f;
                }
                else
                {
                    log_warning("prompt_submit: PROMPT_SEARCH_NEXT: Could not find \"%s\"", result.str);
                    return false;
                }
            }
            else log_warning("prompt_submit: PROMPT_SEARCH_NEXT: Buffer_View %p does not exist", context.go_to_line.for_buffer_view);
        } break;

        case PROMPT_SAVE_AS:
        {
            Buffer_View *buffer_view = context.save_as.for_buffer_view;
            if (view_exists((View *)buffer_view, state))
            {
                file_write(buffer_view->buffer->text_buffer, result.str);
                Buffer *new_buffer = buffer_create_read_file(result.str, state);
                buffer_destroy(buffer_view->buffer, state);
                buffer_view->buffer = new_buffer;
            }
            else log_warning("prompt_submit: PROMPT_SAVE_AS: Buffer_View %p does not exist", context.save_as.for_buffer_view);
        } break;

        case PROMPT_CHANGE_WORKING_DIR:
        {
            return sys_change_working_dir(result.str, state);
        } break;

        default:
        {
            log_warning("prompt_submit: unhandled prompt kind");
        } break;
    }
    return true;
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

Rect get_cursor_rect(Text_Buffer text_buffer, Cursor_Pos cursor_pos, Render_State *render_state)
{
    Rect cursor_rect = get_string_char_rect(text_buffer.lines[cursor_pos.line].str, render_state->font, cursor_pos.col);
    float line_height = get_font_line_height(render_state->font);
    float x = 0;
    float y = cursor_pos.line * line_height;
    cursor_rect.x += x;
    cursor_rect.y += y;
    return cursor_rect;
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

void draw_texture(GLuint texture, Rect q, const unsigned char color[4], Render_State *render_state)
{
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, color));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, color));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, color));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, color));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, color));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 1, 1, color));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
}

void draw_string(const char *str, Render_Font font, float x, float y, const unsigned char color[4], Render_State *render_state)
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
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
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

void draw_frame(Frame frame, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time)
{
    transform_set_canvas_space(canvas_viewport, render_state);
    if (is_active)
        draw_quad(frame.outer_rect, (unsigned char[4]){40, 40, 40, 255});
    else
        draw_quad(frame.outer_rect, (unsigned char[4]){20, 20, 20, 255});

    draw_view(*frame.view, &frame, is_active, canvas_viewport, render_state, delta_time);
}

void draw_view(View view, const Frame *frame, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time)
{
    switch(view.kind)
    {
        case VIEW_KIND_BUFFER:
        {
            draw_buffer_view(view.bv, frame->outer_rect, is_active, canvas_viewport, render_state, delta_time);
        } break;

        case VIEW_KIND_IMAGE:
        {
            draw_image_view(view.iv, render_state);
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            draw_live_scene_view(view.lsv, render_state, delta_time);
        } break;

        default:
        {
            log_warning("draw_view: unhandled View kind: %d", view.kind);
        } break;
    }
}

void draw_buffer_view(Buffer_View buffer_view, Rect frame_rect, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time)
{
    Text_Buffer *text_buffer = &buffer_view.buffer->text_buffer;
    Display_Cursor *display_cursor = &buffer_view.cursor;
    Text_Selection *text_selection = &buffer_view.selection; (void)text_selection;
    Viewport *buffer_viewport = &buffer_view.viewport;

    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, frame_rect, render_state);
    draw_quad(text_area_rect, (unsigned char[4]){10, 10, 10, 255});

    transform_set_buffer_view_text_area(buffer_view, frame_rect, canvas_viewport, render_state);
    draw_text_buffer(*text_buffer, *buffer_viewport, render_state);
    if (is_active)
    {
        draw_cursor(*text_buffer, display_cursor, *buffer_viewport, render_state, delta_time);
    }
    draw_buffer_view_selection(buffer_view, render_state);

    draw_buffer_view_line_numbers(buffer_view, frame_rect, canvas_viewport, render_state);
    if (buffer_view.buffer->kind == BUFFER_KIND_FILE && buffer_view.buffer->file.info.path != NULL)
        draw_buffer_view_name(buffer_view, frame_rect, is_active, canvas_viewport, render_state);
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
            draw_string(text_buffer.lines[i].str, render_state->font, x, y, (unsigned char[4]){255, 255, 255, 255}, render_state);
        y += line_height;
    }
}

void draw_cursor(Text_Buffer text_buffer, Display_Cursor *cursor, Viewport viewport, Render_State *render_state, float delta_time)
{
    cursor->blink_time += delta_time;
    if (cursor->blink_time < 0.5f)
    {
        Rect cursor_rect = get_cursor_rect(text_buffer, cursor->pos, render_state);
        bool is_seen = rect_intersect(cursor_rect, viewport.rect);
        if (is_seen)
            draw_quad(cursor_rect, (unsigned char[4]){0, 0, 255, 255});
    } else if (cursor->blink_time > 1.0f)
    {
        cursor->blink_time -= 1.0f;
    }
}

void draw_buffer_view_selection(Buffer_View buffer_view, Render_State *render_state)
{
    if (buffer_view.mark.active && !cursor_pos_eq(buffer_view.mark.pos, buffer_view.cursor.pos)) {
        Cursor_Pos start = cursor_pos_min(buffer_view.mark.pos, buffer_view.cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view.mark.pos, buffer_view.cursor.pos);
        for (int i = start.line; i <= end.line; i++)
        {
            Text_Line *line = &buffer_view.buffer->text_buffer.lines[i];
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
                if (rect_intersect(selected_rect, buffer_view.viewport.rect))
                    draw_quad(selected_rect, (unsigned char[4]){200, 200, 200, 130});
            }
        }
    }
}

void draw_buffer_view_line_numbers(Buffer_View buffer_view, Rect frame_rect, Viewport canvas_viewport, Render_State *render_state)
{
    transform_set_buffer_view_line_num_col(buffer_view, frame_rect, canvas_viewport, render_state);

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
            draw_string(line_i_str_buf, render_state->font, 0, min_y, color, render_state);
        }
    }
}

void draw_buffer_view_name(Buffer_View buffer_view, Rect frame_rect, bool is_active, Viewport canvas_viewport, Render_State *render_state)
{
    bassert(buffer_view.buffer->kind == BUFFER_KIND_FILE);

    Rect name_rect = buffer_view_get_name_rect(buffer_view, frame_rect, render_state);

    char view_name_buf[256];
    if (!buffer_view.buffer->file.info.has_been_modified)
        snprintf(view_name_buf, sizeof(view_name_buf), "%s", buffer_view.buffer->file.info.path);
    else
        snprintf(view_name_buf, sizeof(view_name_buf), "%s[*]", buffer_view.buffer->file.info.path);

    transform_set_rect(name_rect, canvas_viewport, render_state);
    if (is_active)
        draw_string(view_name_buf, render_state->font, 0, 0, (unsigned char[4]){140, 140, 140, 255}, render_state);
    else
        draw_string(view_name_buf, render_state->font, 0, 0, (unsigned char[4]){100, 100, 100, 255}, render_state);
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

    Frame *active_frame = state->active_frame;
    if (active_frame && active_frame->view->kind == VIEW_KIND_BUFFER)
    {
        Buffer_View *active_buffer_view = &active_frame->view->bv;
        snprintf(status_str_buf, sizeof(status_str_buf),
            "STATUS: Cursor: %d, %d; Line Len: %d; Lines: %d",
            active_buffer_view->cursor.pos.line,
            active_buffer_view->cursor.pos.col,
            active_buffer_view->buffer->text_buffer.lines[active_buffer_view->cursor.pos.line].len,
            active_buffer_view->buffer->text_buffer.line_count);
        draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color, render_state);
        status_str_y += font_line_height;
    }

    snprintf(status_str_buf, sizeof(status_str_buf), "FPS: %3.0f; Delta: %.3f; Working dir: %s", state->fps, state->delta_time, state->working_dir);
    draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color, render_state);
}

void draw_image_view(Image_View image_view, Render_State *render_state)
{
    glUseProgram(render_state->image_shader);
    draw_texture(image_view.image.texture, image_view.image_rect, (unsigned char[4]){255, 255, 255, 255}, render_state);
    glUseProgram(render_state->main_shader);
}

void draw_live_scene_view(Live_Scene_View ls_view, Render_State *render_state, float delta_time)
{
    live_scene_check_hot_reload(ls_view.live_scene);

    glBindFramebuffer(GL_FRAMEBUFFER, ls_view.framebuffer.fbo);
    glViewport(0, 0, ls_view.framebuffer.w, ls_view.framebuffer.h);

    ls_view.live_scene->dylib.render(ls_view.live_scene->state, delta_time);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (int)render_state->framebuffer_dim.x, (int)render_state->framebuffer_dim.y);
    glClearColor(0, 0, 0, 1.0f);
    glBindVertexArray(render_state->vao);

    glUseProgram(render_state->framebuffer_shader);
    draw_texture(ls_view.framebuffer.tex, ls_view.framebuffer_rect, (unsigned char[4]){255, 255, 255, 255}, render_state);
    glUseProgram(render_state->main_shader);
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

void transform_set_buffer_view_text_area(Buffer_View buffer_view, Rect frame_rect, Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16], view_mat_viewport[16], view_mat_canvas[16], view_mat_a[16], view_mat_screen[16], view_mat[16], mvp[16];

    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, frame_rect, render_state);

    make_viewport_transform(buffer_view.viewport, view_mat_viewport);
    make_view(-text_area_rect.x, -text_area_rect.y, 1.0f, view_mat_canvas);
    make_viewport_transform(canvas_viewport, view_mat_screen);
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    mul_mat4(view_mat_viewport, view_mat_canvas, view_mat_a);
    mul_mat4(view_mat_a, view_mat_screen, view_mat);
    mul_mat4(view_mat, proj, mvp);

    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, mvp);

    Rect text_area_screen_rect = canvas_rect_to_screen_rect(text_area_rect, canvas_viewport);
    gl_enable_scissor(text_area_screen_rect, render_state);
}

void transform_set_buffer_view_line_num_col(Buffer_View buffer_view, Rect frame_rect, Viewport canvas_viewport, Render_State *render_state)
{
    float proj[16], view_mat_viewport[16], view_mat_canvas[16], view_mat_a[16], view_mat_screen[16], view_mat[16], mvp[16];

    Rect line_num_col_rect = buffer_view_get_line_num_col_rect(buffer_view, frame_rect, render_state);

    make_view(0.0f, buffer_view.viewport.rect.y, buffer_view.viewport.zoom, view_mat_viewport);
    make_view(-line_num_col_rect.x, -line_num_col_rect.y, 1.0f, view_mat_canvas);
    make_viewport_transform(canvas_viewport, view_mat_screen);
    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    mul_mat4(view_mat_viewport, view_mat_canvas, view_mat_a);
    mul_mat4(view_mat_a, view_mat_screen, view_mat);
    mul_mat4(view_mat, proj, mvp);

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

    glUseProgram(render_state->image_shader);
    glUniformMatrix4fv(render_state->image_shader_mvp_loc, 1, GL_FALSE, mvp);
    glUseProgram(render_state->framebuffer_shader);
    glUniformMatrix4fv(render_state->framebuffer_shader_mvp_loc, 1, GL_FALSE, mvp);
    glUseProgram(render_state->main_shader);
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

    glUseProgram(render_state->image_shader);
    glUniformMatrix4fv(render_state->image_shader_mvp_loc, 1, GL_FALSE, mvp);
    glUseProgram(render_state->framebuffer_shader);
    glUniformMatrix4fv(render_state->framebuffer_shader_mvp_loc, 1, GL_FALSE, mvp);
    glUseProgram(render_state->main_shader);
    glUniformMatrix4fv(render_state->main_shader_mvp_loc, 1, GL_FALSE, mvp);

    glDisable(GL_SCISSOR_TEST);
}

void transform_set_screen_space(Render_State *render_state)
{
    float proj[16];

    make_ortho(0, render_state->window_dim.x, render_state->window_dim.y, 0, -1, 1, proj);

    glUseProgram(render_state->image_shader);
    glUniformMatrix4fv(render_state->image_shader_mvp_loc, 1, GL_FALSE, proj);
    glUseProgram(render_state->framebuffer_shader);
    glUniformMatrix4fv(render_state->framebuffer_shader_mvp_loc, 1, GL_FALSE, proj);
    glUseProgram(render_state->main_shader);
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

Vec_2 get_mouse_delta(GLFWwindow *window, Mouse_State *mouse_state)
{
    Vec_2 current_mouse_pos = get_mouse_screen_pos(window);
    Vec_2 delta_mouse_pos = {
        .x = current_mouse_pos.x - mouse_state->prev_mouse_pos.x,
        .y = current_mouse_pos.y - mouse_state->prev_mouse_pos.y
    };
    return delta_mouse_pos;
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

void viewport_snap_to_cursor(Text_Buffer text_buffer, Cursor_Pos cursor_pos, Viewport *viewport, Render_State *render_state)
{
    Rect viewport_r = viewport->rect;
    Rect_Bounds viewport_b = get_viewport_cursor_bounds(*viewport, render_state->font);
    Rect_Bounds cursor_b = rect_get_bounds(get_cursor_rect(text_buffer, cursor_pos, render_state));
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

bool cursor_iterator_next(Cursor_Iterator *it)
{
    int line = it->pos.line;
    int col = it->pos.col;
    col++;
    if (col > it->buf->lines[line].len - 1)
    {
        if (line < it->buf->line_count - 1)
        {
            line++;
            col = 0;
        }
        else
        {
            return false;
        }
    }
    it->pos.line = line;
    it->pos.col = col;
    return true;
}

bool cursor_iterator_prev(Cursor_Iterator *it)
{
    int line = it->pos.line;
    int col = it->pos.col;
    col--;
    if (col < 0)
    {
        if (line > 0)
        {
            line--;
            col = it->buf->lines[line].len - 1;
        }
        else
        {
            return false;
        }
    }
    it->pos.line = line;
    it->pos.col = col;
    return true;
}

char cursor_iterator_get_char(Cursor_Iterator it)
{
    char c = it.buf->lines[it.pos.line].str[it.pos.col];
    return c;
}

Cursor_Pos cursor_pos_clamp(Text_Buffer text_buffer, Cursor_Pos pos)
{
    if (pos.line < 0)
    {
        pos.line = 0;
        pos.col = 0;
        return pos;
    }
    if (pos.line > text_buffer.line_count - 1)
    {
        pos.line = text_buffer.line_count - 1;
        pos.col = text_buffer.lines[pos.line].len - 1;
        return pos;
    }
    if (pos.col < 0)
    {
        pos.col = 0;
        return pos;
    }
    if (pos.col > text_buffer.lines[pos.line].len - 1)
    {
        pos.col = text_buffer.lines[pos.line].len - 1;
        return pos;
    }
    return pos;
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

Cursor_Pos cursor_pos_advance_char_n(Text_Buffer text_buffer, Cursor_Pos pos, int n, int dir, bool can_switch_lines)
{
    for (int i = 0; i < n; i++)
    {
        pos = cursor_pos_advance_char(text_buffer, pos, dir, can_switch_lines);
    }
    return pos;
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

Cursor_Pos cursor_pos_to_start_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos)
{
    (void)text_buffer; (void)cursor_pos;
    Cursor_Pos pos = {0};
    return pos;
}

Cursor_Pos cursor_pos_to_end_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos)
{
    (void)cursor_pos;
    Cursor_Pos pos;
    pos.line = text_buffer.line_count - 1;
    pos.col = text_buffer.lines[pos.line].len - 1;
    return pos;
}

Cursor_Pos cursor_pos_to_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos)
{
    (void)text_buffer;
    Cursor_Pos new_pos;
    new_pos.line = pos.line;
    new_pos.col = 0;
    return new_pos;
}

Cursor_Pos cursor_pos_to_end_of_line(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Pos new_pos;
    new_pos.line = pos.line;
    new_pos.col = text_buffer.lines[new_pos.line].len - 1;
    return new_pos;
}

Cursor_Pos cursor_pos_to_next_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Iterator curr_it = { .buf = &text_buffer, .pos = pos };
    Cursor_Iterator prev_it = curr_it;
    while (cursor_iterator_next(&curr_it))
    {
        char curr = cursor_iterator_get_char(curr_it);
        char prev = cursor_iterator_get_char(prev_it);
        if ((isspace(prev) || ispunct(prev)) && isalnum(curr))
        {
            return curr_it.pos;
        }
        prev_it = curr_it;
    }
    return cursor_pos_to_end_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_prev_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Iterator curr_it = { .buf = &text_buffer, .pos = pos };
    cursor_iterator_prev(&curr_it);
    Cursor_Iterator prev_it = curr_it;
    while (cursor_iterator_prev(&curr_it))
    {
        char curr = cursor_iterator_get_char(curr_it);
        char prev = cursor_iterator_get_char(prev_it);
        if ((isspace(curr) || ispunct(curr)) && isalnum(prev))
        {
            return prev_it.pos;
        }
        prev_it = curr_it;
    }
    return cursor_pos_to_start_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_next_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos)
{
    for (int line = pos.line + 1; line < text_buffer.line_count; line++)
    {
        if (is_white_line(text_buffer.lines[line - 1].str) && !is_white_line(text_buffer.lines[line].str))
        {
            return (Cursor_Pos){line, 0};
        }
    }
    return cursor_pos_to_end_of_buffer(text_buffer, pos);
}

Cursor_Pos cursor_pos_to_prev_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos)
{
    for (int line = pos.line - 1; line > 0; line--)
    {
        if (is_white_line(text_buffer.lines[line - 1].str) && !is_white_line(text_buffer.lines[line].str))
        {
            return (Cursor_Pos){line, 0};
        }
    }
    return cursor_pos_to_start_of_buffer(text_buffer, pos);
}

Text_Line text_line_make_dup(const char *str)
{
    Text_Line r;
    r.str = xstrdup(str);
    r.len = strlen(r.str);
    r.buf_len = r.len + 1;
    return r;
}

Text_Line text_line_make_dup_range(const char *str, int start, int count)
{
    Text_Line r;
    r.str = xstrndup(str + start, count);
    r.len = strlen(r.str);
    r.buf_len = r.len + 1;
    return r;
}

Text_Line text_line_make_va(const char *fmt, va_list args)
{
    char str_buf[MAX_CHARS_PER_LINE];
    int written_length = vsnprintf(str_buf, sizeof(str_buf) - 1, fmt, args);
    if (written_length < 0) written_length = 0;
    if (written_length >= (int)(sizeof(str_buf) - 1)) written_length = sizeof(str_buf) - 2;

    str_buf[written_length] = '\n';
    str_buf[written_length + 1] = '\0';

    Text_Line line = text_line_make_dup(str_buf);
    return line;
}

Text_Line text_line_make_f(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Text_Line text_line = text_line_make_va(fmt, args);
    va_end(args);
    return text_line;
}

Text_Line text_line_copy(Text_Line source, int start, int end)
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

void text_line___resize(Text_Line *text_line, int new_size)
{
    text_line->str = xrealloc(text_line->str, new_size + 1);
    text_line->buf_len = new_size + 1;
    text_line->len = new_size;
    text_line->str[new_size] = '\0';
}

void text_line_insert_char(Text_Line *text_line, char c, int insert_index)
{
    bassert(insert_index >= 0);
    bassert(insert_index <= text_line->len);
    text_line___resize(text_line, text_line->len + 1);
    for (int i = text_line->len; i > insert_index; i--)
    {
        text_line->str[i] = text_line->str[i - 1];
    }
    text_line->str[insert_index] = c;
}

void text_line_remove_char(Text_Line *text_line, int remove_index)
{
    bassert(remove_index >= 0);
    bassert(remove_index < text_line->len);
    for (int i = remove_index; i < text_line->len; i++)
    {
        text_line->str[i] = text_line->str[i + 1];
    }
    text_line___resize(text_line, text_line->len - 1);
}

void text_line_insert_range(Text_Line *text_line, const char *range, int insert_index, int insert_count)
{
    bassert(insert_index >= 0);
    bassert(insert_index <= text_line->len);
    bassert(insert_count <= (int)strlen(range));
    text_line___resize(text_line, text_line->len + insert_count);
    for (int i = text_line->len; i >= insert_index + insert_count; i--)
    {
        text_line->str[i] = text_line->str[i - insert_count];
    }
    for (int i = 0; i < insert_count; i++)
    {
        text_line->str[insert_index + i] = *range++;
    }
}

void text_line_remove_range(Text_Line *text_line, int remove_index, int remove_count)
{
    bassert(remove_index >= 0);
    bassert(remove_index + remove_count <= text_line->len);
    for (int i = remove_index; i <= text_line->len - remove_count; i++)
    {
        text_line->str[i] = text_line->str[i + remove_count];
    }
    text_line___resize(text_line, text_line->len - remove_count);
}

int text_line_indent_get_level(Text_Line text_line)
{
    int spaces = 0;
    char *str = text_line.str;
    while (*str)
    {
        if (*str == ' ') spaces++;
        else return spaces;
        str++;
    }
    return 0;
}

int text_line_indent_set_level(Text_Line *text_line, int indent_level)
{
    int current_indent_level = text_line_indent_get_level(*text_line);
    int chars_delta = indent_level - current_indent_level;
    text_line_remove_range(text_line, 0, current_indent_level);
    for (int i = 0; i < indent_level; i++)
    {
        text_line_insert_char(text_line, ' ', 0);
    }
    return chars_delta;
}

int text_line_indent_level_increase(Text_Line *text_line)
{
    int indent_level = text_line_indent_get_level(*text_line);
    int chars_to_add = INDENT_SPACES - (indent_level % INDENT_SPACES);
    for (int i = 0; i < chars_to_add; i++)
    {
        text_line_insert_char(text_line, ' ', 0);
    }
    return chars_to_add;
}

int text_line_indent_level_decrease(Text_Line *text_line)
{
    int indent_level = text_line_indent_get_level(*text_line);
    int chars_to_remove = indent_level % INDENT_SPACES;
    if (indent_level >= INDENT_SPACES && chars_to_remove == 0)
    {
        chars_to_remove = 4;
    }
    text_line_remove_range(text_line, 0, chars_to_remove);
    return chars_to_remove;
}

Text_Buffer text_buffer_create_from_lines(const char *first, ...)
{
    Text_Buffer text_buffer = {0};
    va_list args;
    va_start(args, first);
    const char *line = first;
    while (line)
    {
        text_buffer_append_f(&text_buffer, "%s", line);
        line = va_arg(args, const char *);
    }
    va_end(args);
    return text_buffer;
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

void text_buffer_append_line(Text_Buffer *text_buffer, Text_Line text_line)
{
    text_buffer->line_count++;
    text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
    text_buffer->lines[text_buffer->line_count - 1] = text_line;
}

void text_buffer_insert_line(Text_Buffer *text_buffer, Text_Line new_line, int insert_at)
{
    text_buffer->lines = xrealloc(text_buffer->lines, (text_buffer->line_count + 1) * sizeof(text_buffer->lines[0]));
    for (int i = text_buffer->line_count - 1; i >= insert_at ; i--) {
        text_buffer->lines[i + 1] = text_buffer->lines[i];
    }
    text_buffer->line_count++;
    text_buffer->lines[insert_at] = new_line;
}

void text_buffer_remove_line(Text_Buffer *text_buffer, int remove_at)
{
    free(text_buffer->lines[remove_at].str);
    for (int i = remove_at + 1; i <= text_buffer->line_count - 1; i++) {
        text_buffer->lines[i - 1] = text_buffer->lines[i];
    }
    text_buffer->line_count--;
    if (text_buffer->line_count <= 0)
    {
        text_buffer->line_count = 1;
        text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
        text_buffer->lines[0] = text_line_make_dup("\n");
    }
    else
    {
        text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
    }
}

void text_buffer_append_f(Text_Buffer *text_buffer, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Text_Line text_line =  text_line_make_va(fmt, args);
    va_end(args);
    text_buffer_append_line(text_buffer, text_line);
}

void text_buffer___split_line(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    Text_Line *current_line = &text_buffer->lines[pos.line];
    int chars_moved_to_next_line = current_line->len - pos.col;
    Text_Line new_line = text_line_make_dup_range(current_line->str, pos.col, chars_moved_to_next_line);
    text_line_remove_range(current_line, pos.col, chars_moved_to_next_line);
    text_buffer_insert_line(text_buffer, new_line, pos.line + 1);
}

void text_buffer_insert_char(Text_Buffer *text_buffer, char c, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    if (c == '\n')
    {
        text_buffer___split_line(text_buffer, pos);
    }
    text_line_insert_char(&text_buffer->lines[pos.line], c, pos.col);
}

void text_buffer_remove_char(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    bool deleting_line_break = pos.col == text_buffer->lines[pos.line].len - 1; // Valid text buffer will always have \n at len - 1
    Text_Line *this_line = &text_buffer->lines[pos.line];
    if (deleting_line_break)
    {
        if (pos.line < text_buffer->line_count - 1)
        {
            text_line_remove_char(this_line, pos.col);
            Text_Line *next_line = &text_buffer->lines[pos.line + 1];
            text_line_insert_range(this_line, next_line->str, this_line->len, next_line->len);
            text_buffer_remove_line(text_buffer, pos.line + 1);
        }
    }
    else
    {
        text_line_remove_char(this_line, pos.col);
    }
}

Cursor_Pos text_buffer_insert_range(Text_Buffer *text_buffer, const char *range, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    int range_len = strlen(range);
    bassert(range_len > 0);
    int segment_count = str_get_line_segment_count(range);

    Cursor_Pos end_cursor = pos;
    if (segment_count == 1)
    {
        text_line_insert_range(&text_buffer->lines[pos.line], range, pos.col, range_len);
        end_cursor.col = pos.col + range_len;
    }
    else
    {
        text_buffer___split_line(text_buffer, pos);
        int dest_line_i = pos.line;
        int segment_start = 0;
        for (int i = 0; i < segment_count; i++)
        {
            int segment_end = str_find_next_new_line(range, segment_start);
            if (segment_end != range_len) segment_end++; // If new line, copy the new line char too
            int segment_len = segment_end - segment_start;
            if (i == 0) // first segment
            {
                text_line_insert_range(&text_buffer->lines[dest_line_i], range, pos.col, segment_len);
            }
            else if (i == segment_count - 1) // last segment
            {
                text_line_insert_range(&text_buffer->lines[dest_line_i], range + segment_start, 0, segment_len);
                end_cursor.line = dest_line_i;
                end_cursor.col = segment_len;
            }
            else // middle segments
            {
                Text_Line new_line = text_line_make_dup_range(range, segment_start, segment_len);
                text_buffer_insert_line(text_buffer, new_line, dest_line_i);
            }
            dest_line_i++;
            segment_start = segment_end;
        }
    }
    return end_cursor;
}

void text_buffer_remove_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end)
{
    bassert(start.line < text_buffer->line_count);
    bassert(start.col < text_buffer->lines[start.line].len);
    bassert(end.line < text_buffer->line_count);
    bassert(end.col <= text_buffer->lines[end.line].len);
    bassert((end.line == start.line && end.col > start.col) || end.line > start.line);
    if (start.line == end.line)
    {
        text_line_remove_range(&text_buffer->lines[start.line], start.col, end.col - start.col);
    }
    else
    {
        Text_Line *start_text_line = &text_buffer->lines[start.line];
        Text_Line *end_text_line = &text_buffer->lines[end.line];
        text_line_remove_range(start_text_line, start.col, start_text_line->len - start.col);
        text_line_insert_range(start_text_line, end_text_line->str + end.col, start.col, end_text_line->len - end.col);
        int lines_to_remove = end.line - start.line;
        for (int i = 0; i < lines_to_remove; i++)
        {
            text_buffer_remove_line(text_buffer, start.line + 1); // Keep removing the same index, as lines get shifted up
        }
    }
}

char *text_buffer_extract_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end)
{
    bassert(start.line < text_buffer->line_count);
    bassert(start.col < text_buffer->lines[start.line].len);
    bassert(end.line < text_buffer->line_count);
    bassert(end.col <= text_buffer->lines[end.line].len);
    bassert((end.line == start.line && end.col > start.col) || end.line > start.line);
    String_Builder sb = {0};
    if (start.line == end.line)
    {
        string_builder_append_str_range(&sb,
            text_buffer->lines[start.line].str,
            start.col,
            end.col - start.col);
    }
    else
    {
        string_builder_append_str_range(&sb,
            text_buffer->lines[start.line].str,
            start.col,
            text_buffer->lines[start.line].len - start.col);
        for (int i = start.line + 1; i < end.line; i++)
        {
            string_builder_append_str_range(&sb,
                text_buffer->lines[i].str,
                0,
                text_buffer->lines[i].len);
        }
        string_builder_append_str_range(&sb,
            text_buffer->lines[end.line].str,
            0,
            end.col);
    }
    char *extracted_range = string_builder_compile_and_destroy(&sb);
    return extracted_range;
}

int text_buffer_match_indent(Text_Buffer *text_buffer, int line)
{
    int prev_indent_level;
    if (line > 0) prev_indent_level = text_line_indent_get_level(text_buffer->lines[line - 1]);
    else prev_indent_level = 0;
    text_line_indent_set_level(&text_buffer->lines[line], prev_indent_level);
    return prev_indent_level;
}

int text_buffer_whitespace_cleanup(Text_Buffer *text_buffer)
{
    int cleaned_lines = 0;
    for (int line_i = 0; line_i < text_buffer->line_count; line_i++)
    {
        Text_Line *text_line = &text_buffer->lines[line_i];
        int trailing_spaces = 0;
        for (int i = text_line->len - 2; i >= 0; i--) // len - 2 because \n will always be at the end of a line
        {
            if (text_line->str[i] == ' ') trailing_spaces++;
            else break;
        }
        if (trailing_spaces > 0)
        {
            text_line_remove_range(text_line, text_line->len - 1 - trailing_spaces, trailing_spaces);
            cleaned_lines++;
        }
    }
    return cleaned_lines;
}

bool text_buffer_search_next(Text_Buffer *text_buffer, const char *query, Cursor_Pos from, Cursor_Pos *out_pos)
{
    int col_offset = from.col + 1;
    for (int i = from.line; i < text_buffer->line_count; i++)
    {
        char *match = strstr(text_buffer->lines[i].str + col_offset, query);
        if (match)
        {
            int col = match - text_buffer->lines[i].str;
            out_pos->line = i;
            out_pos->col = col;
            return true;
        }
        col_offset = 0;
    }
    return false;
}

void buffer_view_copy_selected(Buffer_View *buffer_view, Editor_State *state)
{
    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        if (ENABLE_OS_CLIPBOARD)
        {
            char *range = text_buffer_extract_range(&buffer_view->buffer->text_buffer, start, end);
            write_clipboard_mac(range);
            free(range);
        }
        else
        {
            if (state->copy_buffer) free(state->copy_buffer);
            state->copy_buffer = text_buffer_extract_range(&buffer_view->buffer->text_buffer, start, end);
        }
    }
}

void buffer_view_paste(Buffer_View *buffer_view, Editor_State *state)
{
    if (ENABLE_OS_CLIPBOARD)
    {
        char buf[10*1024];
        read_clipboard_mac(buf, sizeof(buf));
        if (strlen(buf) > 0)
        {
            if (buffer_view->mark.active)
            {
                buffer_view_delete_selected(buffer_view);
            }
            Cursor_Pos new_cursor = text_buffer_insert_range(&buffer_view->buffer->text_buffer, buf, buffer_view->cursor.pos);
            buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, new_cursor);
        }
    }
    else
    {
        if (state->copy_buffer)
        {
            if (buffer_view->mark.active)
            {
                buffer_view_delete_selected(buffer_view);
            }
            Cursor_Pos new_cursor = text_buffer_insert_range(&buffer_view->buffer->text_buffer, state->copy_buffer, buffer_view->cursor.pos);
            buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, new_cursor);
        }
    }
}

void buffer_view___set_mark(Buffer_View *buffer_view, Cursor_Pos pos)
{
    buffer_view->mark.active = true;
    buffer_view->mark.pos = pos;
}

void buffer_view___validate_mark(Buffer_View *buffer_view)
{
    if (buffer_view->mark.active && cursor_pos_eq(buffer_view->mark.pos, buffer_view->cursor.pos))
        buffer_view->mark.active = false;
}

void buffer_view___set_cursor_to_pixel_position(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, const Render_State *render_state)
{
    Vec_2 mouse_text_area_pos = buffer_view_canvas_pos_to_text_area_pos(*buffer_view, frame_rect, mouse_canvas_pos, render_state);
    Vec_2 mouse_buffer_pos = buffer_view_text_area_pos_to_buffer_pos(*buffer_view, mouse_text_area_pos);
    Cursor_Pos text_cursor_under_mouse = buffer_pos_to_cursor_pos(mouse_buffer_pos, buffer_view->buffer->text_buffer, render_state);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, text_cursor_under_mouse);
}

void buffer_view_delete_selected(Buffer_View *buffer_view)
{
    bassert(buffer_view->mark.active);
    bassert(!cursor_pos_eq(buffer_view->mark.pos, buffer_view->cursor.pos));
    Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
    Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
    text_buffer_remove_range(&buffer_view->buffer->text_buffer, start, end);
    buffer_view->mark.active = false;
    buffer_view->cursor.pos = start;
}

void buffer_view_insert_indent(Buffer_View *buffer_view, Render_State *render_state)
{
    if (buffer_view->mark.active)
    {
        buffer_view_increase_indent_level(buffer_view, render_state);
    }
    else
    {
        int spaces_to_insert = INDENT_SPACES - buffer_view->cursor.pos.col % INDENT_SPACES;
        for (int i = 0; i < spaces_to_insert; i++)
        {
            text_buffer_insert_char(&buffer_view->buffer->text_buffer, ' ', buffer_view->cursor.pos);
        }
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, spaces_to_insert, +1, false);
        buffer_view->cursor.blink_time = 0.0f;
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, render_state);
    }
}

void buffer_view_increase_indent_level(Buffer_View *buffer_view, Render_State *render_state)
{
    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        for (int i = start.line; i <= end.line; i++)
        {
            int chars_added = text_line_indent_level_increase(&buffer_view->buffer->text_buffer.lines[i]);
            if (i == buffer_view->mark.pos.line) buffer_view->mark.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->mark.pos, chars_added, +1, false);
            if (i == buffer_view->cursor.pos.line) buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_added, +1, false);
        }
    }
    else
    {
        int chars_added = text_line_indent_level_increase(&buffer_view->buffer->text_buffer.lines[buffer_view->cursor.pos.line]);
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_added, +1, false);
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, render_state);
    }
}

void buffer_view_decrease_indent_level(Buffer_View *buffer_view, Render_State *render_state)
{
    if (buffer_view->mark.active)
    {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        for (int i = start.line; i <= end.line; i++)
        {
            int chars_removed = text_line_indent_level_decrease(&buffer_view->buffer->text_buffer.lines[i]);
            if (i == buffer_view->mark.pos.line) buffer_view->mark.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->mark.pos, chars_removed, -1, false);
            if (i == buffer_view->cursor.pos.line) buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_removed, -1, false);
        }
    }
    else
    {
        int chars_removed = text_line_indent_level_decrease(&buffer_view->buffer->text_buffer.lines[buffer_view->cursor.pos.line]);
        buffer_view->cursor.pos = cursor_pos_advance_char_n(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, chars_removed, -1, false);
        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, render_state);
    }
}

void buffer_view_delete_current_line(Buffer_View *buffer_view, Render_State *render_state)
{
    text_buffer_remove_line(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos.line);
    buffer_view->cursor.pos = cursor_pos_to_start_of_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, render_state);
}

void buffer_view_whitespace_cleanup(Buffer_View *buffer_view)
{
    int cleaned_lines = text_buffer_whitespace_cleanup(&buffer_view->buffer->text_buffer);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
    trace_log("buffer_view_whitespace_cleanup: Cleaned up %d lines", cleaned_lines);
}

void buffer_view_handle_backspace(Buffer_View *buffer_view, Render_State *render_state)
{
    if (buffer_view->mark.active)
    {
        buffer_view_delete_selected(buffer_view);
    }
    else
    {
        if (buffer_view->cursor.pos.line > 0 || buffer_view->cursor.pos.col > 0)
        {
            buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1, true);
            text_buffer_remove_char(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            buffer_view->cursor.blink_time = 0.0f;
            viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, render_state);
        }
    }
}

void buffer_view_handle_cursor_movement_keys(Buffer_View *buffer_view, Cursor_Movement_Dir dir, bool is_shift_pressed, bool big_steps, bool start_end, Editor_State *state)
{
    if (is_shift_pressed && !buffer_view->mark.active) buffer_view___set_mark(buffer_view, buffer_view->cursor.pos);

    switch (dir)
    {
        case CURSOR_MOVE_LEFT:
        {
            if (big_steps) buffer_view->cursor.pos = cursor_pos_to_prev_start_of_word(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (start_end) buffer_view->cursor.pos = cursor_pos_to_start_of_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1, true);
        } break;
        case CURSOR_MOVE_RIGHT:
        {
            if (big_steps) buffer_view->cursor.pos = cursor_pos_to_next_start_of_word(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (start_end) buffer_view->cursor.pos = cursor_pos_to_end_of_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1, true);
        } break;
        case CURSOR_MOVE_UP:
        {
            if (big_steps) buffer_view->cursor.pos = cursor_pos_to_prev_start_of_paragraph(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (start_end) buffer_view->cursor.pos = cursor_pos_to_start_of_buffer(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, -1);
        } break;
        case CURSOR_MOVE_DOWN:
        {
            if (big_steps) buffer_view->cursor.pos = cursor_pos_to_next_start_of_paragraph(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else if (start_end) buffer_view->cursor.pos = cursor_pos_to_end_of_buffer(buffer_view->buffer->text_buffer, buffer_view->cursor.pos);
            else buffer_view->cursor.pos = cursor_pos_advance_line(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1);
        } break;
    }

    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
    buffer_view->cursor.blink_time = 0.0f;

    if (is_shift_pressed) buffer_view___validate_mark(buffer_view);
    else buffer_view->mark.active = false;
}

void buffer_view_handle_key(Buffer_View *buffer_view, Frame *frame, GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    switch(key)
    {
        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
        if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            Cursor_Movement_Dir dir = CURSOR_MOVE_UP;
            switch (key)
            {
                case GLFW_KEY_LEFT: dir = CURSOR_MOVE_LEFT; break;
                case GLFW_KEY_RIGHT: dir = CURSOR_MOVE_RIGHT; break;
                case GLFW_KEY_UP: dir = CURSOR_MOVE_UP; break;
                case GLFW_KEY_DOWN: dir = CURSOR_MOVE_DOWN; break;
            }
            buffer_view_handle_cursor_movement_keys(buffer_view, dir, mods & GLFW_MOD_SHIFT, mods & GLFW_MOD_ALT, mods & GLFW_MOD_SUPER, state);
        } break;
        case GLFW_KEY_ENTER: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            switch (buffer_view->buffer->kind)
            {
                case BUFFER_KIND_PROMPT:
                {
                    Prompt_Result prompt_result = prompt_parse_result(buffer_view->buffer->text_buffer);
                    if (prompt_submit(buffer_view->buffer->prompt.context, prompt_result, frame->outer_rect, window, state))
                        frame_destroy(frame, state);
                } break;

                default:
                {
                    buffer_view_handle_char_input(buffer_view, '\n', &state->render_state);
                } break;
            }
        } break;
        case GLFW_KEY_BACKSPACE: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            buffer_view_handle_backspace(buffer_view, &state->render_state);
        } break;
        case GLFW_KEY_TAB: if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            buffer_view_insert_indent(buffer_view, &state->render_state);
        } break;
        case GLFW_KEY_LEFT_BRACKET: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            buffer_view_decrease_indent_level(buffer_view, &state->render_state);
        } break;
        case GLFW_KEY_RIGHT_BRACKET: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            buffer_view_increase_indent_level(buffer_view, &state->render_state);
        } break;
        case GLFW_KEY_C: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            buffer_view_copy_selected(buffer_view, state);
        } break;
        case GLFW_KEY_V: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            buffer_view_paste(buffer_view, state);
        } break;
        case GLFW_KEY_X: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            buffer_view_delete_current_line(buffer_view, &state->render_state);
        } break;
        case GLFW_KEY_R: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (buffer_view->buffer->kind == BUFFER_KIND_FILE && buffer_view->buffer->file.info.path != NULL)
            {
                Buffer *new_buffer = buffer_create_read_file(buffer_view->buffer->file.info.path, state);
                buffer_destroy(buffer_view->buffer, state);
                buffer_view->buffer = new_buffer;
            }
        } break;
        case GLFW_KEY_S: if (mods & GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (buffer_view->buffer->kind == BUFFER_KIND_FILE)
            {
                if (mods & GLFW_MOD_SHIFT || buffer_view->buffer->file.info.path == NULL)
                {
                    Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
                    frame_create_buffer_view_prompt(
                        "Save as:",
                        prompt_create_context_save_as(buffer_view),
                        (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                        state);
                }
                else
                {
                    file_write(buffer_view->buffer->text_buffer, buffer_view->buffer->file.info.path);
                    buffer_view->buffer->file.info.has_been_modified = false;
                }
            }
        } break;
        case GLFW_KEY_EQUAL: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            buffer_view->viewport.zoom += 0.25f;
        } break;
        case GLFW_KEY_MINUS: if (mods == GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            buffer_view->viewport.zoom -= 0.25f;
        } break;
        case GLFW_KEY_G: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            frame_create_buffer_view_prompt(
                "Go to line:",
                prompt_create_context_go_to_line(buffer_view),
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                state);
        } break;
        case GLFW_KEY_F: if (mods & GLFW_MOD_SUPER && (action == GLFW_PRESS || action == GLFW_REPEAT))
        {
            if (mods == (GLFW_MOD_SHIFT | GLFW_MOD_SUPER))
            {
                if (state->prev_search)
                {
                    Cursor_Pos found_pos;
                    bool found = text_buffer_search_next(&buffer_view->buffer->text_buffer, state->prev_search, buffer_view->cursor.pos, &found_pos);
                    if (found)
                    {
                        buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, found_pos);
                        viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, &state->render_state);
                        buffer_view->cursor.blink_time = 0.0f;
                    }
                }
            }
            else if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
            {
                Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
                Frame *prompt_frame = frame_create_buffer_view_prompt(
                    "Search next:",
                    prompt_create_context_search_next(buffer_view),
                    (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                    state);
                if (state->prev_search)
                {
                    Text_Line current_path_line = text_line_make_f("%s", state->prev_search);
                    text_buffer_insert_line(&prompt_frame->view->bv.buffer->text_buffer, current_path_line, 1);
                    prompt_frame->view->bv.cursor.pos = cursor_pos_to_end_of_line(prompt_frame->view->bv.buffer->text_buffer, (Cursor_Pos){1, 0});
                }
            }
        } break;
        case GLFW_KEY_SEMICOLON: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            buffer_view_whitespace_cleanup(buffer_view);
        } break;
    }
}

void view_handle_key(View *view, Frame *frame, GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            buffer_view_handle_key(&view->bv, frame, window, state, key, action, mods);
        } break;

        case VIEW_KIND_IMAGE:
        {
            // Nothing for now
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            // Nothing for now
        } break;

        default:
        {
            log_warning("view_handle_key: Unhandled View kind: %d", view->kind);
        } break;
    }
}

void handle_key_input(GLFWwindow *window, Editor_State *state, int key, int action, int mods)
{
    (void) window;
    bool will_propagate_to_view = true;
    switch(key)
    {
        case GLFW_KEY_F1: if (action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            Text_Buffer log_buffer = {0};
            unit_tests_run(&log_buffer, true);
            Frame *frame = frame_create_buffer_view_generic(log_buffer, (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 800, 400}, state);
            frame->view->bv.cursor.pos = cursor_pos_to_end_of_buffer(log_buffer, frame->view->bv.cursor.pos);
            viewport_snap_to_cursor(log_buffer, frame->view->bv.cursor.pos, &frame->view->bv.viewport, &state->render_state);
        } break;
        case GLFW_KEY_F2: if (action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            Frame *frame = frame_create_buffer_view_prompt(
                "Change working dir:",
                prompt_create_context_change_working_dir(),
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 100},
                state);
            Text_Line current_path_line = text_line_make_f("%s", state->working_dir);
            text_buffer_insert_line(&frame->view->bv.buffer->text_buffer, current_path_line, 1);
            frame->view->bv.cursor.pos = cursor_pos_to_end_of_line(frame->view->bv.buffer->text_buffer, (Cursor_Pos){1, 0});
        } break;
        case GLFW_KEY_F5: if (action == GLFW_PRESS)
        {
            Frame *frame = state->active_frame;
            if (frame->view->kind == VIEW_KIND_LIVE_SCENE)
            {
                live_scene_rebuild(frame->view->lsv.live_scene);
            }
            else if (frame->view->kind == VIEW_KIND_BUFFER &&
                frame->view->bv.buffer->kind == BUFFER_KIND_FILE &&
                frame->view->bv.buffer->file.linked_live_scene)
            {
                live_scene_rebuild(frame->view->bv.buffer->file.linked_live_scene);
            }
        } break;
        case GLFW_KEY_F6: if (action == GLFW_PRESS)
        {
            Frame *frame = state->active_frame;
            if (frame->view->kind == VIEW_KIND_BUFFER &&
                frame->view->bv.buffer->kind == BUFFER_KIND_FILE &&
                state->frame_count_ > 1 &&
                state->frames[1]->view->kind == VIEW_KIND_LIVE_SCENE)
            {
                frame->view->bv.buffer->file.linked_live_scene = state->frames[1]->view->lsv.live_scene;
                trace_log("Linked live scene to buffer %s", frame->view->bv.buffer->file.info.path);
            }
        } break;
        case GLFW_KEY_F12: if (action == GLFW_PRESS)
        {
            state->should_break = true;
        } break;
        case GLFW_KEY_F9: if (action == GLFW_PRESS)
        {
            rebuild_dl();
        } break;
        case GLFW_KEY_W: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            if (state->active_frame != NULL)
            {
                frame_destroy(state->active_frame, state);
                will_propagate_to_view = false;
            }
        } break;
        case GLFW_KEY_1: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            frame_create_buffer_view_open_file(
                FILE_PATH1,
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
                state);
        } break;
        case GLFW_KEY_2: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            frame_create_image_view(
                IMAGE_PATH,
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
                state);
        } break;
        case GLFW_KEY_3: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            frame_create_live_scene_view(
                LIVE_CUBE_PATH,
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
                state);
        } break;
        case GLFW_KEY_O: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            frame_create_buffer_view_prompt(
                "Open file:",
                prompt_create_context_open_file(),
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 300, 100},
                state);
        } break;
        case GLFW_KEY_N: if (mods == GLFW_MOD_SUPER && action == GLFW_PRESS)
        {
            Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
            frame_create_buffer_view_empty_file(
                (Rect){mouse_canvas_pos.x, mouse_canvas_pos.y, 500, 500},
                state);
        } break;
    }

    if (will_propagate_to_view && state->active_frame)
    {
        view_handle_key(state->active_frame->view, state->active_frame, window, state, key, action, mods);
    }
}

void buffer_view_handle_char_input(Buffer_View *buffer_view, char c, Render_State *render_state)
{
    if (buffer_view->mark.active)
    {
        buffer_view_delete_selected(buffer_view);
    }
    text_buffer_insert_char(&buffer_view->buffer->text_buffer, c, buffer_view->cursor.pos);
    if (c == '\n')
    {
        int indent_level = text_buffer_match_indent(&buffer_view->buffer->text_buffer, buffer_view->cursor.pos.line + 1);
        buffer_view->cursor.pos = cursor_pos_clamp(
            buffer_view->buffer->text_buffer,
            (Cursor_Pos){buffer_view->cursor.pos.line + 1, indent_level});
    }
    else
    {
        buffer_view->cursor.pos = cursor_pos_advance_char(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, +1, true);
    }
    buffer_view->cursor.blink_time = 0.0f;
    viewport_snap_to_cursor(buffer_view->buffer->text_buffer, buffer_view->cursor.pos, &buffer_view->viewport, render_state);
}

void buffer_view_handle_click_drag(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    (void)is_shift_pressed;
    buffer_view___set_cursor_to_pixel_position(buffer_view, frame_rect, mouse_canvas_pos, render_state);
}

void view_handle_click_drag(View *view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        buffer_view_handle_click_drag(&view->bv, frame_rect, mouse_canvas_pos, is_shift_pressed, render_state);
    }
}

void frame_handle_drag(Frame *frame, Vec_2 drag_delta, Render_State *render_state)
{
    Rect new_rect = frame->outer_rect;
    new_rect.x += drag_delta.x;
    new_rect.y += drag_delta.y;
    frame_set_rect(frame, new_rect, render_state);
}

void frame_handle_resize(Frame *frame, Vec_2 drag_delta, Render_State *render_state)
{
    Rect new_rect = frame->outer_rect;
    new_rect.w += drag_delta.x;
    new_rect.h += drag_delta.y;
    frame_set_rect(frame, new_rect, render_state);
}

void handle_mouse_click_drag(Vec_2 mouse_canvas_pos, Vec_2 mouse_delta, bool is_shift_pressed, Mouse_State *mouse_state, Render_State *render_state)
{
    if (mouse_state->drag_in_view_frame)
    {
        view_handle_click_drag(mouse_state->drag_in_view_frame->view, mouse_state->drag_in_view_frame->outer_rect, mouse_canvas_pos, is_shift_pressed, render_state);
    }
    else if (mouse_state->dragged_frame)
    {
        frame_handle_drag(mouse_state->dragged_frame, mouse_delta, render_state);
    }
    else if (mouse_state->resized_frame)
    {
        frame_handle_resize(mouse_state->resized_frame, mouse_delta, render_state);
    }
}

bool buffer_view_handle_mouse_click(Buffer_View *buffer_view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(*buffer_view, frame_rect, render_state);
    if (rect_p_intersect(mouse_canvas_pos, text_area_rect))
    {
        if (is_shift_pressed)
        {
            if (!buffer_view->mark.active)
                buffer_view___set_mark(buffer_view, buffer_view->cursor.pos);

            buffer_view___set_cursor_to_pixel_position(buffer_view, frame_rect, mouse_canvas_pos, render_state);

            buffer_view___validate_mark(buffer_view);
        }
        else
        {
            buffer_view___set_cursor_to_pixel_position(buffer_view, frame_rect, mouse_canvas_pos, render_state);
            buffer_view___set_mark(buffer_view, buffer_view->cursor.pos);
        }
        return true;
    }
    return false;
}

bool view_handle_mouse_click(View *view, Rect frame_rect, Vec_2 mouse_canvas_pos, bool is_shift_pressed, Render_State *render_state)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        return buffer_view_handle_mouse_click(&view->bv, frame_rect, mouse_canvas_pos, is_shift_pressed, render_state);
    }
    return false;
}

void frame_handle_mouse_click(Frame *frame, Vec_2 mouse_canvas_pos, Mouse_State *mouse_state, Render_State *render_state, bool is_shift_pressed, bool will_propagate_to_view)
{
    Rect resize_handle_rect = frame_get_resize_handle_rect(*frame, render_state);
    if (rect_p_intersect(mouse_canvas_pos, resize_handle_rect))
        mouse_state->resized_frame = frame;
    else if (will_propagate_to_view && view_handle_mouse_click(frame->view, frame->outer_rect, mouse_canvas_pos, is_shift_pressed, render_state))
        mouse_state->drag_in_view_frame = frame;
    else
        mouse_state->dragged_frame = frame;
}

void handle_mouse_click(GLFWwindow *window, Editor_State *state)
{
    Vec_2 mouse_screen_pos = get_mouse_screen_pos(window);
    Vec_2 mouse_canvas_pos = screen_pos_to_canvas_pos(mouse_screen_pos, state->canvas_viewport);
    bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    Frame *clicked_frame = frame_at_pos(mouse_canvas_pos, state);
    if (clicked_frame != NULL)
    {
        if (state->active_frame != clicked_frame)
        {
            frame_set_active(clicked_frame, state);
            frame_handle_mouse_click(clicked_frame, mouse_canvas_pos, &state->mouse_state, &state->render_state, is_shift_pressed, false);
        }
        else
        {
            frame_handle_mouse_click(clicked_frame, mouse_canvas_pos, &state->mouse_state, &state->render_state, is_shift_pressed, true);
        }
    }
}

void buffer_view_handle_mouse_release(Buffer_View *buffer_view)
{
    buffer_view___validate_mark(buffer_view);
}

void view_handle_mouse_release(View *view)
{
    if (view->kind == VIEW_KIND_BUFFER)
    {
        buffer_view_handle_mouse_release(&view->bv);
    }
}

void handle_mouse_release(Mouse_State *mouse_state)
{
    mouse_state->resized_frame = NULL;
    mouse_state->dragged_frame = NULL;
    mouse_state->scrolled_frame = NULL;
    if (mouse_state->drag_in_view_frame)
    {
        view_handle_mouse_release(mouse_state->drag_in_view_frame->view);
        mouse_state->drag_in_view_frame = NULL;
    }
}

void handle_mouse_input(GLFWwindow *window, Editor_State *state)
{
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        bool is_shift_pressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        Vec_2 mouse_delta = get_mouse_delta(window, &state->mouse_state);
        Vec_2 mouse_canvas_pos = get_mouse_canvas_pos(window, state);
        handle_mouse_click_drag(mouse_canvas_pos, mouse_delta, is_shift_pressed, &state->mouse_state, &state->render_state);
    }
}

void string_builder_append_f(String_Builder *string_builder, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    bassert(len >= 0);

    char *chunk = malloc(len + 1);
    vsnprintf(chunk, len + 1, fmt, args);
    va_end(args);

    string_builder->chunk_count++;
    string_builder->chunks = xrealloc(string_builder->chunks, string_builder->chunk_count * sizeof(string_builder->chunks[0]));
    string_builder->chunks[string_builder->chunk_count - 1] = chunk;
}

void string_builder_append_str_range(String_Builder *string_builder, const char *str, int start, int count)
{
    string_builder->chunk_count++;
    string_builder->chunks = xrealloc(string_builder->chunks, string_builder->chunk_count * sizeof(string_builder->chunks[0]));
    string_builder->chunks[string_builder->chunk_count - 1] = xstrndup(str + start, count);
}

char *string_builder_compile_and_destroy(String_Builder *string_builder)
{
    int total_len = 0;
    for (int i = 0; i < string_builder->chunk_count; i++)
    {
        total_len += strlen(string_builder->chunks[i]);
    }

    char *compiled_str = xmalloc(total_len + 1);
    char *compiled_str_ptr = compiled_str;

    for (int i = 0; i < string_builder->chunk_count; i++)
    {
        strcpy(compiled_str_ptr, string_builder->chunks[i]);
        compiled_str_ptr += strlen(string_builder->chunks[i]);
    }

    free(string_builder->chunks);
    string_builder->chunks = NULL;
    string_builder->chunk_count = 0;

    return compiled_str;
}

bool file_read_into_text_buffer(const char *path, Text_Buffer *text_buffer, File_Info *file_info)
{
    file_info->path = xstrdup(path);
    FILE *f = fopen(file_info->path, "r");
    if (!f)
    {
        trace_log("Could not open file at %s", path);
        return false;
    }
    char buf[MAX_CHARS_PER_LINE];
    memset(text_buffer, 0, sizeof(*text_buffer));
    while (fgets(buf, sizeof(buf), f))
    {
        text_buffer->line_count++;
        bassert(text_buffer->line_count <= MAX_LINES);
        text_buffer->lines = xrealloc(text_buffer->lines, text_buffer->line_count * sizeof(text_buffer->lines[0]));
        text_buffer->lines[text_buffer->line_count - 1] = text_line_make_dup(buf);
    }
    fclose(f);
    return true;
}

void file_write(Text_Buffer text_buffer, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) fatal("Failed to open file for writing at %s", path);
    for (int i = 0; i < text_buffer.line_count; i++)
    {
        fputs(text_buffer.lines[i].str, f);
    }
    fclose(f);
    trace_log("Saved file to %s", path);
}

Image file_open_image(const char *path)
{
    Image image = {0};
    int width, height, channels;
    unsigned char *data = stbi_load(path, &width, &height, &channels, 0);
    if (!data) return image;

    image.width = (float)width;
    image.height = (float)height;
    image.channels = channels;

    GLenum format = (channels == 3) ? GL_RGB : GL_RGBA;

    glGenTextures(1, &image.texture);
    glBindTexture(GL_TEXTURE_2D, image.texture);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return image;
}

void read_clipboard_mac(char *buf, size_t buf_size)
{
    FILE *pipe = popen("pbpaste", "r");
    if (!pipe) return;
    size_t len = 0;
    while (fgets(buf + len, buf_size - len, pipe))
    {
        len = strlen(buf);
        if (len >= buf_size - 1) break;
    }
    pclose(pipe);
}

void write_clipboard_mac(const char *text)
{
    FILE *pipe = popen("pbcopy", "w");
    if (!pipe) return;
    fputs(text, pipe);
    pclose(pipe);
}

void rebuild_dl()
{
    int result = system("make dl");
    if (result != 0)
    {
        fprintf(stderr, "Build failed with code %d\n", result);
    }
    trace_log("Rebuilt dl");
}

Live_Scene_Dylib live_scene_load_dylib(const char *path)
{
    Live_Scene_Dylib dylib = {0};
    dylib.dl_handle = xdlopen(path);
    dylib.dl_path = xstrdup(path);
    dylib.dl_timestamp = get_file_timestamp(path);
    dylib.init = xdlsym(dylib.dl_handle, "on_init");
    dylib.reload = xdlsym(dylib.dl_handle, "on_reload");
    dylib.render = xdlsym(dylib.dl_handle, "on_render");
    dylib.destroy = xdlsym(dylib.dl_handle, "on_destroy");
    return dylib;
}

void live_scene_rebuild(Live_Scene *live_scene)
{
    char command_buf[1024];
    snprintf(command_buf, sizeof(command_buf), "make %s", live_scene->dylib.dl_path);
    int result = system(command_buf);
    if (result != 0)
    {
        log_warning("live_scene_rebuild: Build failed with code %d for dylib %s", result, live_scene->dylib.dl_path);
    }
    trace_log("live_scene_rebuild: Rebuilt dylib (%s)", live_scene->dylib.dl_path);
}

bool file___is_image(const char *path)
{
    int x, y, comp;
    return stbi_info(path, &x, &y, &comp) != 0;
}

File_Kind file_detect_kind(const char *path)
{
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".dylib") == 0) return FILE_KIND_DYLIB;
    if (file___is_image(path)) return FILE_KIND_IMAGE;
    return FILE_KIND_TEXT;
}

char *sys_get_working_dir()
{
    char *dir = xmalloc(1024);
    getcwd(dir, 1024);
    return dir;
}

bool sys_change_working_dir(const char *dir, Editor_State *state)
{
    if (chdir(dir) == 0)
    {
        if (state->working_dir) free(state->working_dir);
        state->working_dir = sys_get_working_dir();
        return true;
    }
    log_warning("Failed to change working dir to %s", dir);
    return false;
}
