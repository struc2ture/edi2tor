#include "editor.h"

#include "common.h"
#include "scene_loader.h"

#include <ctype.h>
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
#include "misc.h"
#include "platform_types.h"
#include "shaders.h"
#include "util.h"

void on_init(Editor_State *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene, GLuint fbo)
{
    bassert(sizeof(*state) < 4096);

    state->is_live_scene = is_live_scene;
    if (!is_live_scene) glfwSetWindowTitle(window, "edi2tor");

    state->window = window;

    initialize_render_state(&state->render_state, window_w, window_h, window_px_w, window_px_h, fbo);

    state->canvas_viewport.zoom = 1.0f;
    viewport_set_outer_rect(&state->canvas_viewport, (Rect){0, 0, state->render_state.window_dim.x, state->render_state.window_dim.y});

    state->working_dir = sys_get_working_dir();
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
    }
}

void on_destroy(Editor_State *state)
{
    // TODO: When live scene pointers are stored in an array, use that
    for (int i = 0; i < state->view_count; i++)
    {
        if (state->views[i]->kind == VIEW_KIND_LIVE_SCENE)
        {
            live_scene_destroy(state->views[i]->lsv.live_scene);
        }
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

        draw_grid((Vec_2){state->canvas_viewport.rect.x, state->canvas_viewport.rect.y}, 50, &state->render_state);

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

    if (buffer_view->buffer->kind == BUFFER_KIND_FILE && buffer_view->buffer->file.info.path != NULL)
    {
        mat_stack_push(&render_state->mat_stack_model_view);
        {
            Rect name_rect = buffer_view_get_name_rect(buffer_view, render_state);

            mat_stack_mul_r(&render_state->mat_stack_model_view,
                mat4_translate(name_rect.x, name_rect.y, 0));

            mvp_update_from_stacks(render_state);

            render_view_buffer_name(buffer_view, is_active, canvas_viewport, render_state);
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
            draw_string(text_buffer.lines[i].str, render_state->font, x, y, (Color){255, 255, 255, 255}, render_state);
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

void render_view_buffer_name(Buffer_View *buffer_view, bool is_active, Viewport canvas_viewport, const Render_State *render_state)
{
    bassert(buffer_view->buffer->kind == BUFFER_KIND_FILE && buffer_view->buffer->file.info.path != NULL);

    glUseProgram(render_state->font_shader);

    char view_name_buf[256];
    if (!buffer_view->buffer->file.info.has_been_modified)
        snprintf(view_name_buf, sizeof(view_name_buf), "%s", buffer_view->buffer->file.info.path);
    else
        snprintf(view_name_buf, sizeof(view_name_buf), "%s[*]", buffer_view->buffer->file.info.path);

    if (is_active)
        draw_string(view_name_buf, render_state->font, 0, 0, (Color){140, 140, 140, 255}, render_state);
    else
        draw_string(view_name_buf, render_state->font, 0, 0, (Color){100, 100, 100, 255}, render_state);
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
    glUseProgram(render_state->flipped_quad_shader);
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

    Vec_2 mouse_screen_pos = screen_pos_to_canvas_pos(state->mouse_state.pos, state->canvas_viewport);

    snprintf(status_str_buf, sizeof(status_str_buf), "FPS: %3.0f; Delta: %.3f; Working dir: %s; M: " VEC2_FMT, t->fps_avg, t->prev_delta_time, state->working_dir, VEC2_ARG(mouse_screen_pos));
    draw_string(status_str_buf, render_state->font, status_str_x, status_str_y, status_str_color, render_state);
}

// ---------------------------------------------------------------------------------------------------------------------------

// TODO: Move this out of this file?
void draw_quad(Rect q, Color c, const Render_State *render_state)
{
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 0, 0, c));
    glUseProgram(render_state->quad_shader);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_texture(GLuint texture, Rect q, Color c, const Render_State *render_state)
{
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       1, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 1, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 1, 1, c));
    glUseProgram(render_state->tex_shader);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

void draw_string(const char *str, Render_Font font, float x, float y, Color c, const Render_State *render_state)
{
    // NOTE: Assumes glUseProgram(font_shader) is called outside
    y += font.ascent * font.i_dpi_scale;
    Vert_Buffer vert_buf = {0};
    while (*str)
    {
        if (*str >= 32)
        {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font.char_data, font.atlas_w, font.atlas_h, *str-32, &x, &y ,&q, 1, font.i_dpi_scale);
            vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y0, q.s0, q.t0, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y0, q.s1, q.t0, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x0, q.y1, q.s0, q.t1, c));
            vert_buffer_add_vert(&vert_buf, make_vert(q.x1, q.y1, q.s1, q.t1, c));
        }
        str++;
    }
    glBufferData(GL_ARRAY_BUFFER, VERT_MAX * sizeof(Vert), NULL, GL_DYNAMIC_DRAW); // Orphan existing buffer to stay on the fast path on mac
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
    glBindTexture(GL_TEXTURE_2D, render_state->white_texture);
}

void draw_grid(Vec_2 offset, float spacing, const Render_State *render_state)
{
    glUseProgram(render_state->grid_shader);
    glUniform2f(render_state->grid_shader_resolution_loc, render_state->framebuffer_dim.x, render_state->framebuffer_dim.y);
    float scaled_offset_x = offset.x * render_state->dpi_scale;
    float scaled_offset_y = offset.y * render_state->dpi_scale;
    glUniform2f(render_state->grid_shader_offset_loc, scaled_offset_x, scaled_offset_y);
    glUniform1f(render_state->grid_shader_spacing_loc, spacing * render_state->dpi_scale);

    Rect q = {0, 0, render_state->window_dim.x, render_state->window_dim.y};
    Color c = {0};
    Vert_Buffer vert_buf = {0};
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y,       0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x,       q.y + q.h, 0, 0, c));
    vert_buffer_add_vert(&vert_buf, make_vert(q.x + q.w, q.y + q.h, 0, 0, c));
    glBufferSubData(GL_ARRAY_BUFFER, 0, vert_buf.vert_count * sizeof(vert_buf.verts[0]), vert_buf.verts);
    glDrawArrays((GL_TRIANGLES), 0, vert_buf.vert_count);
}

// ---------------------------------------------------------------------------------------------------------------------------

void mvp_update_from_stacks(Render_State *render_state)
{
    Mat_4 mvp;
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
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(mvp), mvp.m);
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
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Mat_4), NULL, GL_DYNAMIC_DRAW);
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
    }
}

View *create_buffer_view_generic(Text_Buffer text_buffer, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_generic(text_buffer, state);
    Buffer_View *buffer_view = buffer_view_create(buffer, rect, state);
    view_set_active(outer_view(buffer_view), state);
    return outer_view(buffer_view);
}

View *create_buffer_view_open_file(const char *file_path, Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_read_file(file_path, state);
    if (buffer != NULL)
    {
        Buffer_View *buffer_view = buffer_view_create(buffer, rect, state);
        view_set_active(outer_view(buffer_view), state);
        return outer_view(buffer_view);
    }
    return NULL;
}

View *create_buffer_view_empty_file(Rect rect, Editor_State *state)
{
    Buffer *buffer = buffer_create_empty_file(state);
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
            live_scene_destroy(view->lsv.live_scene);
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

View *view_at_pos(Vec_2 pos, Editor_State *state)
{
    for (int i = 0; i < state->view_count; i++)
    {
        Rect buffer_view_rect = state->views[i]->outer_rect;
        Rect resize_handle_rect = view_get_resize_handle_rect(state->views[i], &state->render_state);
        bool at_buffer_view_rect = rect_p_intersect(pos, buffer_view_rect);
        bool at_resize_handle = rect_p_intersect(pos, resize_handle_rect);
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

Vec_2 buffer_view_canvas_pos_to_text_area_pos(Buffer_View *buffer_view, Vec_2 canvas_pos, const Render_State *render_state)
{
    Rect text_area_rect = buffer_view_get_text_area_rect(buffer_view, render_state);
    Vec_2 text_area_pos;
    text_area_pos.x = canvas_pos.x - text_area_rect.x;
    text_area_pos.y = canvas_pos.y - text_area_rect.y;
    return text_area_pos;
}

Vec_2 buffer_view_text_area_pos_to_buffer_pos(Buffer_View *buffer_view, Vec_2 text_area_pos)
{
    Vec_2 buffer_pos;
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

Live_Scene *live_scene_create(Editor_State *state, const char *path, float w, float h, GLuint fbo)
{
    Live_Scene *live_scene = xcalloc(sizeof(Live_Scene));
    live_scene->state = xcalloc(4096);
    live_scene->dylib = scene_loader_dylib_open(path);
    live_scene->dylib.on_init(live_scene->state, state->window, w, h, w, h, true, fbo);
    return live_scene;
}

void live_scene_check_hot_reload(Live_Scene *live_scene)
{
    if (scene_loader_dylib_check_and_hotreload(&live_scene->dylib))
    {
        live_scene->dylib.on_reload(live_scene->state);
    }
}

void live_scene_destroy(Live_Scene *live_scene)
{
    live_scene->dylib.on_destroy(live_scene->state);
    free(live_scene->state);
    scene_loader_dylib_close(&live_scene->dylib);
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
        case PROMPT_OPEN_FILE:
        {
            Rect new_view_rect =
            {
                .x = prompt_rect.x,
                .y = prompt_rect.y,
                .w = 800,
                .h = 600
            };
            File_Kind file_kind = file_detect_kind(result.str);
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
                    create_buffer_view_open_file(result.str, new_view_rect, state);
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

Vec_2 canvas_pos_to_screen_pos(Vec_2 canvas_pos, Viewport canvas_viewport)
{
    Vec_2 screen_pos;
    screen_pos.x = (canvas_pos.x - canvas_viewport.rect.x) * canvas_viewport.zoom;
    screen_pos.y = (canvas_pos.y - canvas_viewport.rect.y) * canvas_viewport.zoom;
    return screen_pos;
}

Vec_2 screen_pos_to_canvas_pos(Vec_2 screen_pos, Viewport canvas_viewport)
{
    Vec_2 canvas_pos;
    canvas_pos.x = canvas_viewport.rect.x + screen_pos.x / canvas_viewport.zoom;
    canvas_pos.y = canvas_viewport.rect.y + screen_pos.y / canvas_viewport.zoom;
    return canvas_pos;
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

Cursor_Pos cursor_pos_to_indent_or_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos)
{
    int col = text_buffer_line_indent_get_level(&text_buffer, pos.line);
    if (col >= pos.col) col = 0;
    Cursor_Pos new_pos;
    new_pos.line = pos.line;
    new_pos.col = col;
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

Cursor_Pos cursor_pos_to_next_end_of_word(Text_Buffer text_buffer, Cursor_Pos pos)
{
    Cursor_Iterator curr_it = { .buf = &text_buffer, .pos = pos };
    Cursor_Iterator prev_it = curr_it;
    while (cursor_iterator_next(&curr_it))
    {
        char curr = cursor_iterator_get_char(curr_it);
        char prev = cursor_iterator_get_char(prev_it);
        if (isalnum(prev) && (isspace(curr) || ispunct(curr)))
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

char text_buffer_remove_char(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    bassert(pos.col < text_buffer->lines[pos.line].len);
    bool deleting_line_break = pos.col == text_buffer->lines[pos.line].len - 1; // Valid text buffer will always have \n at len - 1
    char removed_char = text_buffer->lines[pos.line].str[pos.col];
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
    return removed_char;
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

char text_buffer_get_char(Text_Buffer *text_buffer, Cursor_Pos pos)
{
    bassert(pos.line < text_buffer->line_count);
    Text_Line *line = &text_buffer->lines[pos.line];
    bassert(pos.col < line->len);
    return line->str[pos.col];
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

int text_buffer_line_indent_get_level(Text_Buffer *text_buffer, int line)
{
    int spaces = 0;
    char *str = text_buffer->lines[line].str;
        while (*str)
    {
        if (*str == ' ') spaces++;
        else return spaces;
        str++;
    }
    return 0;
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

void buffer_view_set_cursor_to_pixel_position(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, const Render_State *render_state)
{
    Vec_2 mouse_text_area_pos = buffer_view_canvas_pos_to_text_area_pos(buffer_view, mouse_canvas_pos, render_state);
    Vec_2 mouse_buffer_pos = buffer_view_text_area_pos_to_buffer_pos(buffer_view, mouse_text_area_pos);
    Cursor_Pos text_cursor_under_mouse = buffer_pos_to_cursor_pos(mouse_buffer_pos, buffer_view->buffer->text_buffer, render_state);
    buffer_view->cursor.pos = cursor_pos_clamp(buffer_view->buffer->text_buffer, text_cursor_under_mouse);
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

void live_scene_reset(Editor_State *state, Live_Scene **live_scene, float w, float h, GLuint fbo)
{
    Live_Scene *new_live_scene = live_scene_create(state, (*live_scene)->dylib.original_path, w, h, fbo);
    live_scene_destroy(*live_scene);
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

bool file_is_image(const char *path)
{
    int x, y, comp;
    return stbi_info(path, &x, &y, &comp) != 0;
}

File_Kind file_detect_kind(const char *path)
{
    if (!sys_file_exists(path)) return FILE_KIND_NONE;
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".dylib") == 0) return FILE_KIND_DYLIB;
    if (file_is_image(path)) return FILE_KIND_IMAGE;
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

bool sys_file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

// -------------------------------------------------

Rect_Bounds rect_get_bounds(Rect r)
{
    Rect_Bounds b;
    b.min_x = r.x;
    b.min_y = r.y;
    b.max_x = r.x + r.w;
    b.max_y = r.y + r.h;
    return b;
}

bool rect_intersect(Rect a, Rect b)
{
    float a_min_x = a.x;
    float a_min_y = a.y;
    float a_max_x = a.x + a.w;
    float a_max_y = a.y + a.h;
    float b_min_x = b.x;
    float b_min_y = b.y;
    float b_max_x = b.x + b.w;
    float b_max_y = b.y + b.h;
    bool intersect =
        a_min_x < b_max_x && a_max_x > b_min_x &&
        a_min_y < b_max_y && a_max_y > b_min_y;
    return intersect;
}

bool rect_p_intersect(Vec_2 p, Rect rect)
{
    float min_x = rect.x;
    float min_y = rect.y;
    float max_x = rect.x + rect.w;
    float max_y = rect.y + rect.h;
    bool intersect =
        p.x > min_x && p.x < max_x &&
        p.y > min_y && p.y < max_y;
    return intersect;
}

bool cursor_pos_eq(Cursor_Pos a, Cursor_Pos b)
{
    bool equal = a.line == b.line && a.col == b.col;
    return equal;
}

Cursor_Pos cursor_pos_min(Cursor_Pos a, Cursor_Pos b)
{
    if (a.line < b.line || (a.line == b.line && a.col <= b.col)) return a;
    else return b;
}

Cursor_Pos cursor_pos_max(Cursor_Pos a, Cursor_Pos b)
{
    if (a.line > b.line || (a.line == b.line && a.col >= b.col)) return a;
    else return b;
}

#include "actions.c"
#include "input.c"
#include "misc.c"
#include "scene_loader.c"
#include "unit_tests.c"
