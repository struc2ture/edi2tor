#include "editor.h"

#include "actions.h"
#include "common.h"
#include "text_buffer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "input.h"
#include "history.h"
#include "misc.h"
#include "os.h"
#include "platform_types.h"
#include "renderer.h"
#include "scene_loader.h"
#include "shaders.h"
#include "text_buffer.h"
#include "util.h"

void on_init(Editor_State *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene, GLuint fbo, int argc, char **argv)
{
    bassert(sizeof(*state) < 4096);

    state->is_live_scene = is_live_scene;
    if (!is_live_scene) glfwSetWindowTitle(window, "edi2tor");

    state->window = window;

    initialize_render_state(&state->render_state, window_w, window_h, window_px_w, window_px_h, fbo);

    state->canvas_viewport.zoom = 1.0f;
    viewport_set_outer_rect(&state->canvas_viewport, (Rect){0, 0, state->render_state.window_dim.x, state->render_state.window_dim.y});

    state->buffer_seed = 1;

    // Working dir can be passed as the third arg to platform
    // e.g. bin/platform bin/editor.dylib /Users/user/project
    if (argc > 2)
    {
        os_change_working_dir(argv[2], state);
    }
    else
    {
        state->working_dir = os_get_working_dir();
    }

    action_load_workspace(state);
}

void on_reload(Editor_State *state)
{
    (void)state;
}

void on_frame(Editor_State *state, const Platform_Timing *t)
{
    if (state->should_break)
    {
        __builtin_debugtrap();
        state->should_break = false;
    }

    input_mouse_update(state, t->prev_delta_time);

    editor_render(state, t);
}

void on_platform_event(Editor_State *state, const Platform_Event *e)
{
    (void)state; (void)e;

    if (state->input_capture_live_scene_view)
    {
        if (e->kind == PLATFORM_EVENT_KEY && e->key.key == GLFW_KEY_F10 && e->key.action == GLFW_PRESS)
        {
            action_live_scene_toggle_capture_input(state);
            return;
        }

        Live_Scene *ls = state->input_capture_live_scene_view->live_scene;\
        Platform_Event adjusted_event = input__adjust_mouse_event_for_live_scene_view(state, state->input_capture_live_scene_view, e);
        ls->dylib.on_platform_event(ls->state, &adjusted_event);

        return;
    }

    switch (e->kind)
    {
        case PLATFORM_EVENT_CHAR:
        {
            input_char_global(state, e);
        } break;

        case PLATFORM_EVENT_KEY:
        {
            input_key_global(state, e);
        } break;

        case PLATFORM_EVENT_MOUSE_BUTTON:
        {
            input_mouse_button_global(state, e);
        } break;

        case PLATFORM_EVENT_MOUSE_MOTION:
        {
            input_mouse_motion_global(state, e);
        } break;

        case PLATFORM_EVENT_MOUSE_SCROLL:
        {
            input_mouse_scroll_global(state, e);
        } break;

        case PLATFORM_EVENT_WINDOW_RESIZE:
        {
            state->render_state.window_dim.x = e->window_resize.logical_w;
            state->render_state.window_dim.y = e->window_resize.logical_h;
            state->render_state.framebuffer_dim.x = e->window_resize.px_w;
            state->render_state.framebuffer_dim.y = e->window_resize.px_h;
            state->render_state.dpi_scale = state->render_state.framebuffer_dim.x / state->render_state.window_dim.x;
            // glViewport(0, 0, state->render_state.framebuffer_dim.x, state->render_state.framebuffer_dim.y);
        } break;

        default: break;
    }
}

void on_destroy(Editor_State *state)
{
    action_save_workspace(state);

    for (int i = 0; i < state->live_scene_count; i++)
    {
        live_scene_destroy(state->live_scenes[i], state);
    }
}

// ------------------------------------------------------------------------------------------------------------------------

void editor_render(Editor_State *state, const Platform_Timing *t)
{
    glViewport(0, 0, (GLsizei)state->render_state.framebuffer_dim.x, (GLsizei)state->render_state.framebuffer_dim.y);

    glClearColor(0.4f, 0.3f, 0.1f, 1.0f);
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindVertexArray(state->render_state.vao);
    glBindBuffer(GL_ARRAY_BUFFER, state->render_state.vbo);

    mat_stack_push(&state->render_state.mat_stack_proj);
    {
        mat_stack_mul_r(&state->render_state.mat_stack_proj,
            mat4_proj_ortho(0, state->render_state.window_dim.x, state->render_state.window_dim.y, 0, -1, 1));

        mvp_update_from_stacks(&state->render_state);

        draw_grid(V2(state->canvas_viewport.rect.x, state->canvas_viewport.rect.y), 50, &state->render_state);

        mat_stack_push(&state->render_state.mat_stack_model_view);
        {
            mat_stack_mul_r(&state->render_state.mat_stack_model_view,
                mat4_translate(-state->canvas_viewport.rect.x, -state->canvas_viewport.rect.y, 0));
            mat_stack_mul_r(&state->render_state.mat_stack_model_view,
                mat4_scale(state->canvas_viewport.zoom, state->canvas_viewport.zoom, 1));

            mvp_update_from_stacks(&state->render_state);

            // Render backwards for correct z ordering
            for (int i = state->view_count - 1; i >= 0; i--)
            {
                View *view = state->views[i];
                bool is_active = view == state->active_view;
                render_view(view, is_active, state->canvas_viewport, &state->render_state, t);
            }
        }
        mat_stack_pop(&state->render_state.mat_stack_model_view);
        mvp_update_from_stacks(&state->render_state);

        render_status_bar(state, &state->render_state, t);
    }
    mat_stack_pop(&state->render_state.mat_stack_proj);
    mvp_update_from_stacks(&state->render_state);

    bassert(state->render_state.mat_stack_proj.size == 0);
    bassert(state->render_state.mat_stack_model_view.size == 0);
    // TODO: Can I query GL scissor state, for validation? and other glEnable things...
}

void render_view(View *view, bool is_active, Viewport canvas_viewport, Render_State *render_state, const Platform_Timing *t)
{
    if (is_active)
        draw_quad(view->outer_rect, (Color){40, 40, 40, 255}, render_state);
    else
        draw_quad(view->outer_rect, (Color){20, 20, 20, 255}, render_state);

    switch(view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            render_view_buffer(&view->bv, is_active, canvas_viewport, render_state, t->prev_delta_time);
        } break;

        case VIEW_KIND_IMAGE:
        {
            render_view_image(&view->iv, render_state);
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            render_view_live_scene(&view->lsv, render_state, t);
        } break;
    }
}

void render_view_buffer(Buffer_View *buffer_view, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time)
{
    Text_Buffer *text_buffer = &buffer_view->buffer->text_buffer;
    Display_Cursor *display_cursor = &buffer_view->cursor;
    Viewport *buffer_viewport = &buffer_view->viewport;

    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, render_state);
    draw_quad(text_area_rect, (Color){10, 10, 10, 255}, render_state);

    mat_stack_push(&render_state->mat_stack_model_view);
    {
        mat_stack_mul_r(&render_state->mat_stack_model_view,
            mat4_translate(text_area_rect.x, text_area_rect.y, 0));
        mat_stack_mul_r(&render_state->mat_stack_model_view,
            mat4_scale(buffer_view->viewport.zoom, buffer_view->viewport.zoom, 1));
        mat_stack_mul_r(&render_state->mat_stack_model_view,
            mat4_translate(-buffer_view->viewport.rect.x, -buffer_view->viewport.rect.y, 0));

        mvp_update_from_stacks(render_state);

        Rect text_area_screen_rect = canvas_rect_to_screen_rect(text_area_rect, canvas_viewport);
        gl_enable_scissor(text_area_screen_rect, render_state);
        {
            render_view_buffer_text(*text_buffer, *buffer_viewport, render_state);
            if (is_active)
            {
                render_view_buffer_cursor(*text_buffer, display_cursor, *buffer_viewport, render_state, delta_time);
            }
            render_view_buffer_selection(buffer_view, render_state);
        }
        gl_disable_scissor();
    }
    mat_stack_pop(&render_state->mat_stack_model_view);

    mat_stack_push(&render_state->mat_stack_model_view);
    {
        Rect line_num_col_rect = buffer_view_get_line_num_col_rect(buffer_view, render_state);

        mat_stack_mul_r(&render_state->mat_stack_model_view,
            mat4_translate(line_num_col_rect.x, line_num_col_rect.y, 0));
        mat_stack_mul_r(&render_state->mat_stack_model_view,
            mat4_scale(buffer_view->viewport.zoom, buffer_view->viewport.zoom, 1));
        mat_stack_mul_r(&render_state->mat_stack_model_view,
            mat4_translate(0, -buffer_view->viewport.rect.y, 0));

        mvp_update_from_stacks(render_state);

        Rect line_num_col_screen_rect = canvas_rect_to_screen_rect(line_num_col_rect, canvas_viewport);
        gl_enable_scissor(line_num_col_screen_rect, render_state);
        {

            render_view_buffer_line_numbers(buffer_view, canvas_viewport, render_state);
        }
        gl_disable_scissor();
    }
    mat_stack_pop(&render_state->mat_stack_model_view);

    if (buffer_view->buffer->file_path)
    {
        mat_stack_push(&render_state->mat_stack_model_view);
        {
            Rect name_rect = buffer_view_get_name_rect(buffer_view, render_state);

            mat_stack_mul_r(&render_state->mat_stack_model_view,
                mat4_translate(name_rect.x, name_rect.y, 0));

            mvp_update_from_stacks(render_state);

            render_view_buffer_name(buffer_view, buffer_view->buffer->file_path, is_active, canvas_viewport, render_state);
        }
        mat_stack_pop(&render_state->mat_stack_model_view);
    }

    mvp_update_from_stacks(render_state);
}

void render_view_buffer_text(Text_Buffer text_buffer, Viewport viewport, const Render_State *render_state)
{
    glUseProgram(render_state->font_shader);

    float x = 0, y = 0;
    float line_height = get_font_line_height(render_state->font);

    for (int i = 0; i < text_buffer.line_count; i++)
    {
        Rect string_rect = get_string_rect(text_buffer.lines[i].str, render_state->font, 0, y);
        bool is_seen = rect_intersect(string_rect, viewport.rect);
        if (is_seen)
            draw_string(text_buffer.lines[i].str, render_state->font, x, y, render_state->text_color, render_state);
        y += line_height;
    }
}

void render_view_buffer_cursor(Text_Buffer text_buffer, Display_Cursor *cursor, Viewport viewport,  const Render_State *render_state, float delta_time)
{
    cursor->blink_time += delta_time;
    if (cursor->blink_time < 0.5f)
    {
        Rect cursor_rect = get_cursor_rect(text_buffer, cursor->pos, render_state);
        bool is_seen = rect_intersect(cursor_rect, viewport.rect);
        if (is_seen)
            draw_quad(cursor_rect, (Color){100, 100, 255, 180}, render_state);
    } else if (cursor->blink_time > 1.0f)
    {
        cursor->blink_time -= 1.0f;
    }
}

void render_view_buffer_selection(Buffer_View *buffer_view, const Render_State *render_state)
{
    if (buffer_view->mark.active && !cursor_pos_eq(buffer_view->mark.pos, buffer_view->cursor.pos)) {
        Cursor_Pos start = cursor_pos_min(buffer_view->mark.pos, buffer_view->cursor.pos);
        Cursor_Pos end = cursor_pos_max(buffer_view->mark.pos, buffer_view->cursor.pos);
        for (int i = start.line; i <= end.line; i++)
        {
            Text_Line *line = &buffer_view->buffer->text_buffer.lines[i];
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
                if (rect_intersect(selected_rect, buffer_view->viewport.rect))
                    draw_quad(selected_rect, (Color){200, 200, 200, 130}, render_state);
            }
        }
    }
}

void render_view_buffer_line_numbers(Buffer_View *buffer_view, Viewport canvas_viewport, const Render_State *render_state)
{
    const float font_line_height = get_font_line_height(render_state->font);
    const float viewport_min_y = buffer_view->viewport.rect.y;
    const float viewport_max_y = viewport_min_y + buffer_view->viewport.rect.h;

    glUseProgram(render_state->font_shader);

    char line_i_str_buf[256];
    for (int line_i = 0; line_i < buffer_view->buffer->text_buffer.line_count; line_i++)
    {
        const float min_y = font_line_height * line_i;
        const float max_y = min_y + font_line_height;
        if (min_y < viewport_max_y && max_y > viewport_min_y)
        {
            snprintf(line_i_str_buf, sizeof(line_i_str_buf), "%3d", line_i + 1);
            Color c;
            if (line_i != buffer_view->cursor.pos.line)
            {
                c = (Color){150, 150, 150, 255};
            }
            else
            {
                c = (Color){230, 230, 230, 255};
            }
            draw_string(line_i_str_buf, render_state->font, 0, min_y, c, render_state);
        }
    }
}

void render_view_buffer_name(Buffer_View *buffer_view, const char *name, bool is_active, Viewport canvas_viewport, const Render_State *render_state)
{
    glUseProgram(render_state->font_shader);

    if (is_active)
        draw_string(name, render_state->font, 0, 0, (Color){140, 140, 140, 255}, render_state);
    else
        draw_string(name, render_state->font, 0, 0, (Color){100, 100, 100, 255}, render_state);
}

void render_view_image(Image_View *image_view, const Render_State *render_state)
{
    glUseProgram(render_state->tex_shader);
    draw_texture(image_view->image.texture, image_view->image_rect, (Color){255, 255, 255, 255}, render_state);
}

void render_view_live_scene(Live_Scene_View *ls_view, const Render_State *render_state, const Platform_Timing *t)
{
    // TODO: Keep pointers to live scenes in an array and run live scene updates separately, before rendering
    live_scene_check_hot_reload(ls_view->live_scene);

    glBindFramebuffer(GL_FRAMEBUFFER, ls_view->framebuffer.fbo);

    ls_view->live_scene->dylib.on_frame(ls_view->live_scene->state, t);

    glBindFramebuffer(GL_FRAMEBUFFER, render_state->default_fbo);
    glViewport(0, 0, (int)render_state->framebuffer_dim.x, (int)render_state->framebuffer_dim.y);
    glClearColor(0, 0, 0, 1.0f);
    glBindVertexArray(render_state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, render_state->vbo);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    Rect q = ls_view->framebuffer_rect;
    Color c = {255, 255, 255, 255};
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 1, 1, c));
    glUseProgram(render_state->flipped_quad_shader);
    glBindTexture(GL_TEXTURE_2D, ls_view->framebuffer.tex);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void render_status_bar(Editor_State *state, const Render_State *render_state, const Platform_Timing *t)
{
    const float font_line_height = get_font_line_height(render_state->font);
    const float x_padding = 10;
    const float y_padding = 2;
    const float status_bar_height = 2 * font_line_height + 2 * y_padding;

    const Rect status_bar_rect = {
        .x = 0,
        .y = render_state->window_dim.y - status_bar_height,
        .w = render_state->window_dim.x,
        .h = render_state->window_dim.y};

    draw_quad(status_bar_rect, (Color){30, 30, 30, 255}, render_state);

    char status_str_buf[256];
    const Color status_str_color = {200, 200, 200, 255};

    float status_str_x = status_bar_rect.x + x_padding;
    float status_str_y = status_bar_rect.y + y_padding;

    glUseProgram(render_state->font_shader);

    View *active_view = state->active_view;
    if (active_view && active_view->kind == VIEW_KIND_BUFFER)
    {
        Buffer_View *active_buffer_view = &active_view->bv;
        snprintf(status_str_buf, sizeof(status_str_buf),
            "STATUS: Cursor: %d, %d; Line Len: %d; Lines: %d",
            active_buffer_view->cursor.pos.line,
            active_buffer_view->cursor.pos.col,
            active_buffer_view->buffer->text_buffer.lines[active_buffer_view->cursor.pos.line].len,
            active_buffer_view->buffer->text_buffer.line_count);
        draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color, render_state);
        status_str_y += font_line_height;
    }

    v2 mouse_screen_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);

    snprintf(status_str_buf, sizeof(status_str_buf), "FPS: %3.0f; Delta: %.3f; Working dir: %s; M: <%.2f, %.2f>", t->fps_avg, t->prev_delta_time, state->working_dir, mouse_screen_pos.x, mouse_screen_pos.y);
    draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color, render_state);
}

void mvp_update_from_stacks(Render_State *render_state)
{
    m4 mvp;
    if (render_state->mat_stack_proj.size > 0 && render_state->mat_stack_model_view.size > 0)
    {
        mvp = mat4_mul(mat_stack_peek(&render_state->mat_stack_proj), mat_stack_peek(&render_state->mat_stack_model_view));
    }
    else if (render_state->mat_stack_proj.size > 0)
    {
        mvp = mat_stack_peek(&render_state->mat_stack_proj);
    }
    else
    {
        mvp = mat4_identity();
    }
    glBindBuffer(GL_UNIFORM_BUFFER, render_state->mvp_ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mvp), mvp.d);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void initialize_render_state(Render_State *render_state, float window_w, float window_h, float window_px_w, float window_px_h, GLuint fbo)
{
    render_state->window_dim.x = window_w;
    render_state->window_dim.y = window_h;
    render_state->framebuffer_dim.x = window_px_w;
    render_state->framebuffer_dim.y = window_px_h;
    render_state->dpi_scale = render_state->framebuffer_dim.x / render_state->window_dim.x;

    render_state->default_fbo = fbo;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    render_state->font_shader = gl_create_shader_program(shader_vert_quad_src, shader_frag_font_src);
    render_state->grid_shader = gl_create_shader_program(shader_vert_quad_src, shader_frag_grid_src);
    render_state->quad_shader = gl_create_shader_program(shader_vert_quad_src, shader_frag_quad_src);
    render_state->tex_shader = gl_create_shader_program(shader_vert_quad_src, shader_frag_tex_src);
    render_state->flipped_quad_shader = gl_create_shader_program(shader_vert_flipped_quad_src, shader_frag_tex_src);

    render_state->grid_shader_offset_loc = glGetUniformLocation(render_state->grid_shader, "u_offset");
    render_state->grid_shader_spacing_loc = glGetUniformLocation(render_state->grid_shader, "u_spacing");
    render_state->grid_shader_resolution_loc = glGetUniformLocation(render_state->grid_shader, "u_resolution");

    glGenBuffers(1, &render_state->mvp_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, render_state->mvp_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(m4), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    GLuint mvp_ubo_binding_point = 0;
    glBindBufferBase(GL_UNIFORM_BUFFER, mvp_ubo_binding_point, render_state->mvp_ubo);
    glUniformBlockBinding(render_state->font_shader, glGetUniformBlockIndex(render_state->font_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->grid_shader, glGetUniformBlockIndex(render_state->grid_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->quad_shader, glGetUniformBlockIndex(render_state->quad_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->tex_shader, glGetUniformBlockIndex(render_state->tex_shader, "Matrices"), mvp_ubo_binding_point);
    glUniformBlockBinding(render_state->flipped_quad_shader, glGetUniformBlockIndex(render_state->flipped_quad_shader, "Matrices"), mvp_ubo_binding_point);

    glGenVertexArrays(1, &render_state->vao);
    glGenBuffers(1, &render_state->vbo);
    glBindVertexArray(render_state->vao);
    glBindBuffer(GL_ARRAY_BUFFER, render_state->vbo);
    glBufferData(GL_ARRAY_BUFFER,  VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vert), (void *)offsetof(Vert, u));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vert), (void *)offsetof(Vert, c));
    glEnableVertexAttribArray(2);

    glActiveTexture(GL_TEXTURE0);
    render_state->font = load_font(FONT_PATH, render_state->dpi_scale);
    glActiveTexture(GL_TEXTURE1); // Make sure any subsequent bind texture calls won't affect the font texture slot

    glUseProgram(render_state->tex_shader);
    glUniform1i(glGetUniformLocation(render_state->tex_shader, "u_tex"), 1);
    glUseProgram(render_state->flipped_quad_shader);
    glUniform1i(glGetUniformLocation(render_state->flipped_quad_shader, "u_tex"), 1);

    render_state->buffer_view_line_num_col_width = get_string_rect("000", render_state->font, 0, 0).w;
    render_state->buffer_view_name_height = get_font_line_height(render_state->font);
    render_state->buffer_view_padding = 6.0f;
    render_state->buffer_view_resize_handle_radius = 5.0f;
    render_state->text_color = (Color){255, 255, 255, 255};

    glUseProgram(0);
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

Buffer *buffer_create_empty(Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);

    Buffer *buffer = xcalloc(sizeof(Buffer));

    buffer->id = state->buffer_seed++;
    buffer->text_buffer = text_buffer_create_empty();

    *new_slot = buffer;
    return *new_slot;
}

void buffer_replace_text_buffer(Buffer *buffer, Text_Buffer text_buffer)
{
    text_buffer_destroy(&buffer->text_buffer);
    buffer->text_buffer = text_buffer;
}

void buffer_replace_file(Buffer *buffer, const char *path)
{
    if (buffer->file_path) free(buffer->file_path);
    buffer->file_path = xstrdup(path);
}

Buffer *buffer_create_prompt(const char *prompt_text, Prompt_Context context, Editor_State *state)
{
    Buffer **new_slot = buffer_create_new_slot(state);

    Buffer *buffer = xcalloc(sizeof(Buffer));
    buffer->id = state->buffer_seed++;

    buffer->text_buffer.line_count = 2;
    buffer->text_buffer.lines = xmalloc(buffer->text_buffer.line_count * sizeof(buffer->text_buffer.lines[0]));
    buffer->prompt_context = context;

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
    text_buffer_destroy(&buffer->text_buffer);
    buffer_free_slot(buffer, state);
    free(buffer);
}

Buffer *buffer_get_by_id(Editor_State *state, int id)
{
    for (int i = 0; i < state->buffer_count; i++)
    {
        if (state->buffers[i]->id == id) return state->buffers[i];
    }
    return NULL;
}

View *create_buffer_view_generic(Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_empty(state);
    Buffer_View *buffer_view = buffer_view_create(buffer, rect, state);
    view_set_active(outer_view(buffer_view), state);
    return outer_view(buffer_view);
}

View *create_buffer_view_open_file(const char *path, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_empty(state);

    Text_Buffer tb;
    bool read_success = text_buffer_read_from_file(path, &tb);
    if (read_success)
    {
        buffer_replace_text_buffer(buffer, tb);
        buffer->file_path = xstrdup(path);
    }
    else
    {
        buffer_destroy(buffer, state);
        return NULL;
    }

    Buffer_View *buffer_view = buffer_view_create(buffer, rect, state);
    view_set_active(outer_view(buffer_view), state);
    return outer_view(buffer_view);
}

View *create_buffer_view_prompt(const char *prompt_text, Prompt_Context context, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_prompt(prompt_text, context, state);
    Buffer_View *buffer_view = buffer_view_create(buffer, rect, state);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, (Cursor_Pos){2, 0});
    view_set_active(outer_view(buffer_view), state);
    return outer_view(buffer_view);
}

View *create_image_view(const char *file_path, Rect rect, Editor_State *state)
{
    Image image = file_open_image(file_path);
    Image_View *image_view = image_view_create(image, rect, state);
    view_set_active(outer_view(image_view), state);
    return outer_view(image_view);
}

View *create_live_scene_view(const char *dylib_path, Rect rect, Editor_State *state)
{
    Gl_Framebuffer framebuffer = gl_create_framebuffer((int)rect.w, (int)rect.h);
    Live_Scene *live_scene = live_scene_create(state, dylib_path, rect.w, rect.h, framebuffer.fbo);
    Live_Scene_View *live_scene_view = live_scene_view_create(framebuffer, live_scene, rect, state);
    view_set_active(outer_view(live_scene_view), state);
    return outer_view(live_scene_view);
}

int view_get_index(View *view, Editor_State *state)
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

View **view_create_new_slot(Editor_State *state)
{
    state->view_count++;
    state->views = xrealloc(state->views, state->view_count * sizeof(state->views[0]));
    return &state->views[state->view_count - 1];
}

void view_free_slot(View *view, Editor_State *state)
{
    int index_to_delete = view_get_index(view, state);
    for (int i = index_to_delete; i < state->view_count - 1; i++)
    {
        state->views[i] = state->views[i + 1];
    }
    state->view_count--;
    state->views = xrealloc(state->views, state->view_count * sizeof(state->views[0]));
}

View *view_create(Editor_State *state)
{
    View **new_slot = view_create_new_slot(state);
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
            view_free_slot(view, state);
            free(view);
        } break;

        case VIEW_KIND_IMAGE:
        {
            image_destroy(view->iv.image);
            view_free_slot(view, state);
            free(view);
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            gl_destroy_framebuffer(&view->lsv.framebuffer);
            live_scene_destroy(view->lsv.live_scene, state);
            view_free_slot(view, state);
            free(view);
        } break;
    }

    if (state->active_view == view)
        state->active_view = state->view_count > 0 ? state->views[0] : NULL;
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

Rect view_get_inner_rect(View *view, const Render_State *render_state)
{
    Rect rect;
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            rect = buffer_view_get_text_area_rect(&view->bv, render_state);
        } break;

        case VIEW_KIND_LIVE_SCENE:
        {
            rect = view->lsv.framebuffer_rect;
        } break;

        default:
        {
            rect = (Rect){0};
        } break;
    }
    return rect;
}

void view_set_rect(View *view, Rect rect, const Render_State *render_state)
{
    view->outer_rect = rect;
    switch (view->kind)
    {
        case VIEW_KIND_BUFFER:
        {
            Rect text_area_rect = buffer_view_get_text_area_rect(&view->bv, render_state);
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

            Platform_Event resize_event;
            resize_event.kind = PLATFORM_EVENT_WINDOW_RESIZE;
            resize_event.window_resize.logical_w = view->lsv.framebuffer_rect.w;
            resize_event.window_resize.logical_h = view->lsv.framebuffer_rect.h;
            // TODO: Actually recreate framebuffer texture if size changed
            resize_event.window_resize.px_w = (float)view->lsv.framebuffer.w;
            resize_event.window_resize.px_h = (float)view->lsv.framebuffer.h;
            view->lsv.live_scene->dylib.on_platform_event(view->lsv.live_scene->state, &resize_event);
        } break;
    }
}

void view_set_active(View *view, Editor_State *state)
{
    int active_index = view_get_index(view, state);
    for (int i = active_index; i > 0; i--)
    {
        state->views[i] = state->views[i - 1];
    }
    state->views[0] = view;
    state->active_view = view;
}

Rect view_get_resize_handle_rect(View *view, const Render_State *render_state)
{
    Rect r;
    r.x = view->outer_rect.x + view->outer_rect.w - render_state->buffer_view_resize_handle_radius;
    r.y = view->outer_rect.y + view->outer_rect.h - render_state->buffer_view_resize_handle_radius;
    r.w = 2 * render_state->buffer_view_resize_handle_radius;
    r.h = 2 * render_state->buffer_view_resize_handle_radius;
    return r;
}

View *view_at_pos(v2 pos, Editor_State *state)
{
    for (int i = 0; i < state->view_count; i++)
    {
        Rect buffer_view_rect = state->views[i]->outer_rect;
        Rect resize_handle_rect = view_get_resize_handle_rect(state->views[i], &state->render_state);
        bool at_buffer_view_rect = rect_contains_p(pos, buffer_view_rect);
        bool at_resize_handle = rect_contains_p(pos, resize_handle_rect);
        if (at_buffer_view_rect || at_resize_handle)
        {
            return state->views[i];
        }
    }
    return NULL;
}

Buffer_View *buffer_view_create(Buffer *buffer, Rect rect, Editor_State *state)
{
    View *view = view_create(state);
    view->kind = VIEW_KIND_BUFFER;
    view->bv.viewport.zoom = DEFAULT_ZOOM;
    view->bv.buffer = buffer;
    view_set_rect(view, rect, &state->render_state);
    return (Buffer_View *)view;
}

Rect buffer_view_get_text_area_rect(Buffer_View *buffer_view, const Render_State *render_state)
{
    (void)buffer_view;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect outer_rect = outer_view(buffer_view)->outer_rect;
    Rect r;
    r.x = outer_rect.x + pad + line_num_w + pad;
    r.y = outer_rect.y + pad + name_h + pad;
    r.w = outer_rect.w - pad - line_num_w - pad - pad;
    r.h = outer_rect.h - pad - name_h - pad - pad;
    return r;
}

Rect buffer_view_get_line_num_col_rect(Buffer_View *buffer_view, const Render_State *render_state)
{
    (void)buffer_view;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect outer_rect = outer_view(buffer_view)->outer_rect;
    Rect r;
    r.x = outer_rect.x + pad;
    r.y = outer_rect.y + pad + name_h + pad;
    r.w = line_num_w;
    r.h = outer_rect.h - pad - name_h - pad - pad;
    return r;
}

Rect buffer_view_get_name_rect(Buffer_View *buffer_view, const Render_State *render_state)
{
    (void)buffer_view;
    const float right_limit = 40.0f;
    const float pad = render_state->buffer_view_padding;
    const float line_num_w = render_state->buffer_view_line_num_col_width;
    const float name_h = render_state->buffer_view_name_height;
    Rect outer_rect = outer_view(buffer_view)->outer_rect;
    Rect r;
    r.x = outer_rect.x + pad + line_num_w + pad;
    r.y = outer_rect.y + pad;
    r.w = outer_rect.w - pad - line_num_w - pad - pad - right_limit;
    r.h = name_h;
    return r;
}

v2 buffer_view_canvas_pos_to_text_area_pos(Buffer_View *buffer_view, v2 canvas_pos, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, render_state);
    v2 text_area_pos;
    text_area_pos.x = canvas_pos.x - text_area_rect.x;
    text_area_pos.y = canvas_pos.y - text_area_rect.y;
    return text_area_pos;
}

v2 buffer_view_text_area_pos_to_buffer_pos(Buffer_View *buffer_view, v2 text_area_pos)
{
    v2 buffer_pos;
    buffer_pos.x = buffer_view->viewport.rect.x + text_area_pos.x / buffer_view->viewport.zoom;
    buffer_pos.y = buffer_view->viewport.rect.y + text_area_pos.y / buffer_view->viewport.zoom;
    return buffer_pos;
}

void image_destroy(Image image)
{
    glDeleteTextures(1, &image.texture);
}

Image_View *image_view_create(Image image, Rect rect, Editor_State *state)
{
    View *view = view_create(state);
    view->kind = VIEW_KIND_IMAGE;
    view->iv.image = image;
    view_set_rect(view, rect, &state->render_state);
    return (Image_View *)view;
}

Live_Scene **live_scene_create_new_slot(Editor_State *state)
{
    state->live_scene_count++;
    state->live_scenes = xrealloc(state->live_scenes, state->live_scene_count * sizeof(state->live_scenes[0]));
    return &state->live_scenes[state->live_scene_count - 1];
}

int live_scene_get_index(Live_Scene *live_scene, Editor_State *state)
{
    int index = 0;
    Live_Scene **live_scene_c = state->live_scenes;
    while (*live_scene_c != live_scene)
    {
        index++;
        live_scene_c++;
    }
    return index;
}

void live_scene_free_slot(Live_Scene *live_scene, Editor_State *state)
{
    int index_to_delete = live_scene_get_index(live_scene, state);
    for (int i = index_to_delete; i < state->live_scene_count - 1; i++)
    {
        state->live_scenes[i] = state->live_scenes[i + 1];
    }
    state->live_scene_count--;
    state->live_scenes = xrealloc(state->live_scenes, state->live_scene_count * sizeof(state->live_scenes[0]));
}

Live_Scene *live_scene_create(Editor_State *state, const char *path, float w, float h, GLuint fbo)
{
    Live_Scene **new_slot = live_scene_create_new_slot(state);
    Live_Scene *live_scene = xcalloc(sizeof(Live_Scene));
    live_scene->state = xcalloc(4096);
    live_scene->dylib = scene_loader_dylib_open(path);
    live_scene->dylib.on_init(live_scene->state, state->window, w, h, w, h, true, fbo, 0, NULL);
    live_scene->dylib.on_reload(live_scene->state);
    *new_slot = live_scene;
    return *new_slot;
}

void live_scene_check_hot_reload(Live_Scene *live_scene)
{
    if (scene_loader_dylib_check_and_hotreload(&live_scene->dylib))
    {
        live_scene->dylib.on_reload(live_scene->state);
    }
}

void live_scene_destroy(Live_Scene *live_scene, Editor_State *state)
{
    live_scene->dylib.on_destroy(live_scene->state);
    scene_loader_dylib_close(&live_scene->dylib);
    free(live_scene->state);
    live_scene_free_slot(live_scene, state);
    free(live_scene);
}

Live_Scene_View *live_scene_view_create(Gl_Framebuffer framebuffer, Live_Scene *live_scene, Rect rect, Editor_State *state)
{
    View *view = view_create(state);
    view->kind = VIEW_KIND_LIVE_SCENE;
    view->lsv.framebuffer = framebuffer;
    view->lsv.live_scene = live_scene;
    view_set_rect(view, rect, &state->render_state);
    return (Live_Scene_View *)view;
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

bool prompt_submit(Prompt_Context context, Prompt_Result result, Rect prompt_rect, Editor_State *state)
{
    switch (context.kind)
    {
        case PROMPT_NONE: break;

        case PROMPT_OPEN_FILE:
        {
            Rect new_view_rect =
            {
                .x = prompt_rect.x,
                .y = prompt_rect.y,
                .w = 800,
                .h = 600
            };
            File_Kind file_kind = os_file_detect_kind(result.str);
            switch (file_kind)
            {
                case FILE_KIND_IMAGE:
                {
                    create_image_view(result.str, new_view_rect, state);
                } break;
                case FILE_KIND_DYLIB:
                {
                    create_live_scene_view(result.str, new_view_rect, state);
                } break;
                case FILE_KIND_TEXT:
                {
                    if (!create_buffer_view_open_file(result.str, new_view_rect, state))
                        return false;
                } break;
                default: return false;
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
                Buffer *b = buffer_view->buffer;
                text_buffer_history_whitespace_cleanup(&b->text_buffer, &b->history);
                text_buffer_write_to_file(b->text_buffer, result.str);
                buffer_replace_file(b, result.str);
                action_save_workspace(state);
            }
            else log_warning("prompt_submit: PROMPT_SAVE_AS: Buffer_View %p does not exist", context.save_as.for_buffer_view);
        } break;

        case PROMPT_CHANGE_WORKING_DIR:
        {
            return os_change_working_dir(result.str, state);
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

Vert make_vert(float x, float y, float u, float v, Color c)
{
    Vert vert = {x, y, u, v, c};
    return vert;
}

void vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert)
{
    bassert(vert_buffer->vert_count < VERT_MAX);
    vert_buffer->verts[vert_buffer->vert_count++] = vert;
}

Render_Font load_font(const char *path, float dpi_scale)
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
    font.i_dpi_scale = 1 / dpi_scale;
    stbtt_BakeFontBitmap(file_bytes, 0, font.size * dpi_scale, atlas_bitmap, font.atlas_w, font.atlas_h, 32, font.char_count, font.char_data);
    stbtt_GetScaledFontVMetrics(file_bytes, 0, font.size * dpi_scale, &font.ascent, &font.descent, &font.line_gap);
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
    float height = (font.ascent - font.descent) * font.i_dpi_scale;
    return height;
}

float get_char_width(char c, Render_Font font)
{
    float char_width = font.char_data[c - 32].xadvance * font.i_dpi_scale;
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

Rect get_cursor_rect(Text_Buffer text_buffer, Cursor_Pos cursor_pos, const Render_State *render_state)
{
    Rect cursor_rect = get_string_char_rect(text_buffer.lines[cursor_pos.line].str, render_state->font, cursor_pos.col);
    float line_height = get_font_line_height(render_state->font);
    float x = 0;
    float y = cursor_pos.line * line_height;
    cursor_rect.x += x;
    cursor_rect.y += y;
    return cursor_rect;
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

v2 canvas_pos_to_screen_pos(v2 canvas_pos, Viewport canvas_viewport)
{
    v2 screen_pos;
    screen_pos.x = (canvas_pos.x - canvas_viewport.rect.x) * canvas_viewport.zoom;
    screen_pos.y = (canvas_pos.y - canvas_viewport.rect.y) * canvas_viewport.zoom;
    return screen_pos;
}

v2 screen_pos_to_canvas_pos(v2 screen_pos, Viewport canvas_viewport)
{
    v2 canvas_pos;
    canvas_pos.x = canvas_viewport.rect.x + screen_pos.x / canvas_viewport.zoom;
    canvas_pos.y = canvas_viewport.rect.y + screen_pos.y / canvas_viewport.zoom;
    return canvas_pos;
}

Cursor_Pos buffer_pos_to_cursor_pos(v2 buffer_pos, Text_Buffer text_buffer, const Render_State *render_state)
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

void text_buffer_history_insert_char(Text_Buffer *text_buffer, History *history, char c, Cursor_Pos pos)
{
    bool will_add_history = history_get_last_uncommitted_command(history) != NULL;

    text_buffer_insert_char(text_buffer, c, pos);

    if (will_add_history)
    {
        history_add_delta(history, &(Delta){
            .kind = DELTA_INSERT_CHAR,
            .insert_char.pos = pos,
            .insert_char.c = c
        });
    }
}

void text_buffer_history_remove_char(Text_Buffer *text_buffer, History *history, Cursor_Pos pos)
{
    bool will_add_history = history_get_last_uncommitted_command(history) != NULL;

    char removed_char = text_buffer_remove_char(text_buffer, pos);

    if (will_add_history)
    {
        history_add_delta(history, &(Delta){
            .kind = DELTA_REMOVE_CHAR,
            .insert_char.pos = pos,
            .insert_char.c = removed_char
        });
    }
}

Cursor_Pos text_buffer_history_insert_range(Text_Buffer *text_buffer, History *history, const char *range, Cursor_Pos pos)
{
    bool will_add_history = history_get_last_uncommitted_command(history) != NULL;

    Cursor_Pos end = text_buffer_insert_range(text_buffer, range, pos);

    if (will_add_history)
    {
        history_add_delta(history, &(Delta){
            .kind = DELTA_INSERT_RANGE,
            .insert_range.start = pos,
            .insert_range.end = end,
            .insert_range.range = xstrdup(range)
        });
    }

    return end;
}

void text_buffer_history_remove_range(Text_Buffer *text_buffer, History *history, Cursor_Pos start, Cursor_Pos end)
{
    bool will_add_history = history_get_last_uncommitted_command(history) != NULL;

    if (will_add_history)
    {
        history_add_delta(history, &(Delta){
            .kind = DELTA_REMOVE_RANGE,
            .remove_range.start = start,
            .remove_range.end = end,
            .remove_range.range = text_buffer_extract_range(text_buffer, start, end)
        });
    }

    text_buffer_remove_range(text_buffer, start, end);
}

int text_buffer_history_line_indent_increase_level(Text_Buffer *text_buffer, History *history, int line)
{
    int indent_level = text_buffer_line_indent_get_level(text_buffer, line);
    int chars_to_add = INDENT_SPACES - (indent_level % INDENT_SPACES);
    for (int i = 0; i < chars_to_add; i++)
    {
        text_buffer_history_insert_char(text_buffer, history, ' ', (Cursor_Pos){line, 0});
    }
    return chars_to_add;
}

int text_buffer_history_line_indent_decrease_level(Text_Buffer *text_buffer, History *history, int line)
{
    int indent_level = text_buffer_line_indent_get_level(text_buffer, line);
    int chars_to_remove = indent_level % INDENT_SPACES;
    if (indent_level >= INDENT_SPACES && chars_to_remove == 0)
    {
        chars_to_remove = 4;
    }
    if (chars_to_remove > 0)
    {
        text_buffer_history_remove_range(text_buffer, history, (Cursor_Pos){line, 0}, (Cursor_Pos){line, chars_to_remove});
    }
    return chars_to_remove;
}

int text_buffer_history_line_indent_set_level(Text_Buffer *text_buffer, History *history, int line, int indent_level)
{
    int current_indent_level = text_buffer_line_indent_get_level(text_buffer, line);
    if (current_indent_level > 0)
    {
        text_buffer_history_remove_range(text_buffer, history, (Cursor_Pos){line, 0}, (Cursor_Pos){line, current_indent_level});
    }
    for (int i = 0; i < indent_level; i++)
    {
        text_buffer_history_insert_char(text_buffer, history, ' ', (Cursor_Pos){line, 0});
    }
    int chars_delta = indent_level - current_indent_level;
    return chars_delta;
}

int text_buffer_history_line_match_indent(Text_Buffer *text_buffer, History *history, int line)
{
    int prev_indent_level;
    if (line > 0) prev_indent_level = text_buffer_line_indent_get_level(text_buffer, line - 1);
    else prev_indent_level = 0;
    text_buffer_history_line_indent_set_level(text_buffer, history, line, prev_indent_level);
    return prev_indent_level;
}

int text_buffer_history_whitespace_cleanup(Text_Buffer *text_buffer, History *history)
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
            text_buffer_history_remove_range(text_buffer, history, (Cursor_Pos){line_i, text_line->len - 1 - trailing_spaces}, (Cursor_Pos){line_i, text_line->len - 1});
            cleaned_lines++;
        }
    }
    return cleaned_lines;
}

// -------------------------------------------------------------------------------

void buffer_view_set_mark(Buffer_View *buffer_view, Cursor_Pos pos)
{
    buffer_view->mark.active = true;
    buffer_view->mark.pos = pos;
}

void buffer_view_validate_mark(Buffer_View *buffer_view)
{
    if (buffer_view->mark.active && cursor_pos_eq(buffer_view->mark.pos, buffer_view->cursor.pos))
        buffer_view->mark.active = false;
}

void buffer_view_set_cursor_to_pixel_position(Buffer_View *buffer_view, v2 mouse_canvas_pos, const Render_State *render_state)
{
    v2 mouse_text_area_pos = buffer_view_canvas_pos_to_text_area_pos(buffer_view, mouse_canvas_pos, render_state);
    v2 mouse_buffer_pos = buffer_view_text_area_pos_to_buffer_pos(buffer_view, mouse_text_area_pos);
    Cursor_Pos text_cursor_under_mouse = buffer_pos_to_cursor_pos(mouse_buffer_pos, buffer_view->buffer->text_buffer, render_state);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, text_cursor_under_mouse);
}

bool text_buffer_read_from_file(const char *path, Text_Buffer *text_buffer)
{
    FILE *f = fopen(path, "r");
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

void text_buffer_write_to_file(Text_Buffer text_buffer, const char *path)
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

void live_scene_reset(Editor_State *state, Live_Scene **live_scene, float w, float h, GLuint fbo)
{
    Live_Scene *new_live_scene = live_scene_create(state, (*live_scene)->dylib.original_path, w, h, fbo);
    live_scene_destroy(*live_scene, state);
    *live_scene = new_live_scene;
}

void live_scene_rebuild(Live_Scene *live_scene)
{
    char command_buf[1024];
    snprintf(command_buf, sizeof(command_buf), "make %s", live_scene->dylib.original_path);
    int result = system(command_buf);
    if (result != 0)
    {
        log_warning("live_scene_rebuild: Build failed with code %d for dylib %s", result, live_scene->dylib.original_path);
    }
    trace_log("live_scene_rebuild: Rebuilt dylib (%s)", live_scene->dylib.original_path);
}

#include "actions.c"
#include "input.c"
#include "history.c"
#include "misc.c"
#include "os.c"
#include "renderer.c"
#include "scene_loader.c"
#include "scratch_runner.c"
#include "string_builder.c"
#include "text_buffer.c"
#include "unit_tests.c"
