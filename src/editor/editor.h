#pragma once

#include "../lib/common.h"

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stb_truetype.h>

#include "history.h"
#include "../hub/hub.h"
#include "../lib/misc.h"
#include "text_buffer.h"

#define VERT_MAX 8192
#define SCROLL_SENS 10.0f
#define SCROLL_TIMEOUT 0.1f
#define VIEWPORT_CURSOR_BOUNDARY_LINES 5
#define VIEWPORT_CURSOR_BOUNDARY_COLUMNS 5
#define GO_TO_LINE_CHAR_MAX 32
#define MAX_LINES 16384
#define INDENT_SPACES 4
#define DEFAULT_ZOOM 1.0f
#define FONT_PATH "res/UbuntuSansMono-Regular.ttf"
#define FONT_SIZE 18.0f
#define ENABLE_OS_CLIPBOARD true

#define FILE_PATH1 "res/mock7.txt"
// #define FILE_PATH1 "res/mock4.txt"
#define FILE_PATH2 "src/editor.c"
#define IMAGE_PATH "res/DUCKS.png"
#define E2_WORKSPACE ".e2/workspace"
#define E2_TEMP_FILES ".e2/temp_files"

typedef struct {
    unsigned char r, g, b, a;
} Color;

typedef struct {
    float x, y;
    float u, v;
    Color c;
} Vert;

typedef struct {
    Vert verts[VERT_MAX];
    int vert_count;
} Vert_Buffer;

typedef struct {
    GLuint texture;
    float width;
    float height;
    int channels;
} Image;

typedef struct {
    Rect rect;
    float zoom;
} Viewport;

typedef struct {
    Cursor_Pos pos;
    float blink_time;
} Display_Cursor;

typedef struct {
    stbtt_bakedchar *char_data;
    int char_count;
    GLuint texture;
    float size;
    float ascent, descent, line_gap;
    int atlas_w, atlas_h;
    float i_dpi_scale;
} Render_Font;

struct Render_State {
    GLuint font_shader;
    GLuint grid_shader;
    GLuint quad_shader;
    GLuint tex_shader;
    GLuint flipped_quad_shader;
    GLuint vao;
    GLuint vbo;
    GLuint mvp_ubo;
    GLuint grid_shader_offset_loc;
    GLuint grid_shader_spacing_loc;
    GLuint grid_shader_resolution_loc;
    Vec_2 window_dim;
    Vec_2 framebuffer_dim;
    float dpi_scale;
    Render_Font font;
    GLuint white_texture;
    float buffer_view_line_num_col_width;
    float buffer_view_name_height;
    float buffer_view_padding;
    float buffer_view_resize_handle_radius;
    GLuint default_fbo;

    Mat_Stack mat_stack_proj;
    Mat_Stack mat_stack_model_view;

    Color text_color;
};

typedef enum {
    PROMPT_NONE,
    PROMPT_OPEN_FILE,
    PROMPT_SAVE_AS,
    PROMPT_GO_TO_LINE,
    PROMPT_SEARCH_NEXT,
    PROMPT_CHANGE_WORKING_DIR,
} Prompt_Kind;

typedef struct Buffer_View Buffer_View;
typedef struct {
    Prompt_Kind kind;
    union {
    struct {/* nothing for open_file */} open_file;
    struct {
        Buffer_View *for_buffer_view;
    } go_to_line;
    struct {
        Buffer_View *for_buffer_view;
    } save_as;
    };
} Prompt_Context;

typedef struct {
    char str[MAX_CHARS_PER_LINE];
} Prompt_Result;

typedef struct {
    char *file_path;
    Prompt_Context prompt_context;
    History history;
    Text_Buffer text_buffer;
    int id;
} Buffer;

struct Buffer_View {
    Buffer *buffer;
    Viewport viewport;
    Display_Cursor cursor;
    Text_Mark mark;
    bool is_mouse_drag;
};

typedef struct {
    Image image;
    Rect image_rect;
} Image_View;

typedef enum {
    VIEW_KIND_BUFFER,
    VIEW_KIND_IMAGE,
} View_Kind;

typedef struct {
    union {
        Buffer_View bv;
        Image_View iv;
    };
    Rect outer_rect;
    View_Kind kind;
} View;

typedef struct {
    View *resized_view;
    View *dragged_view;
    View *scrolled_view;
    Vec_2 pos;
    float scroll_timeout;
    bool canvas_dragged;
} Mouse_State;

typedef struct {
    Render_State render_state;
    Mouse_State mouse_state;

    Buffer **buffers;
    int buffer_count;
    int buffer_seed;

    View **views;
    int view_count;
    View *active_view;

    Viewport canvas_viewport;

    char *copy_buffer;

    char *working_dir;

    char *prev_search;

    GLFWwindow *window;

    bool should_break;

    struct Hub_Context *hub_context;
} Editor_State;

typedef enum {
    CURSOR_MOVE_LEFT,
    CURSOR_MOVE_RIGHT,
    CURSOR_MOVE_UP,
    CURSOR_MOVE_DOWN
} Cursor_Movement_Dir;

typedef enum {
    FILE_KIND_NONE,
    FILE_KIND_TEXT,
    FILE_KIND_IMAGE,
    FILE_KIND_DYLIB
} File_Kind;

void editor_init(Editor_State *state, struct Hub_Context *hub_context);
void editor_frame(Editor_State *state, const struct Hub_Timing *t);
void editor_event(Editor_State *state, const struct Hub_Event *e);
void editor_destroy(Editor_State *state);

void editor_render(Editor_State *state, const struct Hub_Timing *t);
void render_view(View *view, bool is_active, Viewport canvas_viewport, Render_State *render_state, const struct Hub_Timing *t);
void render_view_buffer(Buffer_View *buffer_view, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time);
void render_view_buffer_text(Text_Buffer text_buffer, Viewport viewport, const Render_State *render_state);
void render_view_buffer_cursor(Text_Buffer text_buffer, Display_Cursor *cursor, Viewport viewport, const Render_State *render_state, float delta_time);
void render_view_buffer_selection(Buffer_View *buffer_view, const Render_State *render_state);
void render_view_buffer_line_numbers(Buffer_View *buffer_view, Viewport canvas_viewport, const Render_State *render_state);
void render_view_buffer_name(Buffer_View *buffer_view, const char *name, bool is_active, Viewport canvas_viewport, const Render_State *render_state);
void render_view_image(Image_View *image_view, const Render_State *render_state);
void render_status_bar(Editor_State *state, const Render_State *render_state, const struct Hub_Timing *t);

void draw_quad(Rect q, Color c, const Render_State *render_state);
void draw_texture(GLuint texture, Rect q, Color c, const Render_State *render_state);
void draw_string(const char *str, Render_Font font, float x, float y, Color c, const Render_State *render_state);
void draw_grid(Vec_2 offset, float spacing, const Render_State *render_state);

void mvp_update_from_stacks(Render_State *render_state);
void initialize_render_state(Render_State *render_state, float window_w, float window_h, float window_px_w, float window_px_h, GLuint fbo);

Buffer **buffer_create_new_slot(Editor_State *state);
void buffer_free_slot(Buffer *buffer, Editor_State *state);
Buffer *buffer_create_empty(Editor_State *state);
void buffer_replace_text_buffer(Buffer *buffer, Text_Buffer text_buffer);
void buffer_replace_file(Buffer *buffer, const char *path);
Buffer *buffer_create_prompt(const char *prompt_text, Prompt_Context context, Editor_State *state);
int buffer_get_index(Buffer *buffer, Editor_State *state);
void buffer_destroy(Buffer *buffer, Editor_State *state);
Buffer *buffer_get_by_id(Editor_State *state, int id);

View *create_buffer_view_generic(Rect rect, Editor_State *state);
View *create_buffer_view_open_file(const char *path, Rect rect, Editor_State *state);
View *create_buffer_view_prompt(const char *prompt_text, Prompt_Context context, Rect rect, Editor_State *state);
View *create_image_view(const char *file_path, Rect rect, Editor_State *state);

int view_get_index(View *view, Editor_State *state);
View **view_create_new_slot(Editor_State *state);
void view_free_slot(View *view, Editor_State *state);
View *view_create(Editor_State *state);
void view_destroy(View *view, Editor_State *state);
bool view_exists(View *view, Editor_State *state);
Rect view_get_inner_rect(View *view, const Render_State *render_state);
void view_set_rect(View *view, Rect rect, const Render_State *render_state);
void view_set_active(View *view, Editor_State *state);
Rect view_get_resize_handle_rect(View *view, const Render_State *render_state);
View *view_at_pos(Vec_2 pos, Editor_State *state);
bool view_handle_scroll(View *view, float x_offset, float y_offset, const Render_State *render_state);
#define outer_view(child_view_ptr) ((View *)(child_view_ptr))

Buffer_View *buffer_view_create(Buffer *buffer, Rect rect, Editor_State *state);
Rect buffer_view_get_text_area_rect(Buffer_View *buffer_view, const Render_State *render_state);
Rect buffer_view_get_line_num_col_rect(Buffer_View *buffer_view, const Render_State *render_state);
Rect buffer_view_get_name_rect(Buffer_View *buffer_view, const Render_State *render_state);
Vec_2 buffer_view_canvas_pos_to_text_area_pos(Buffer_View *buffer_view, Vec_2 canvas_pos, const Render_State *render_state);
Vec_2 buffer_view_text_area_pos_to_buffer_pos(Buffer_View *buffer_view, Vec_2 text_area_pos);

void image_destroy(Image image);

Image_View *image_view_create(Image image, Rect rect, Editor_State *state);

Prompt_Context prompt_create_context_open_file();
Prompt_Context prompt_create_context_go_to_line(Buffer_View *for_buffer_view);
Prompt_Context prompt_create_context_search_next(Buffer_View *for_buffer_view);
Prompt_Context prompt_create_context_save_as(Buffer_View *for_buffer_view);
Prompt_Context prompt_create_context_change_working_dir();
Prompt_Context prompt_create_context_set_action_scratch_buffer_id(Buffer_View *for_buffer_view);
Prompt_Result prompt_parse_result(Text_Buffer text_buffer);
bool prompt_submit(Prompt_Context context, Prompt_Result result, Rect prompt_rect, Editor_State *state);

void viewport_set_outer_rect(Viewport *viewport, Rect outer_rect);
void viewport_set_zoom(Viewport *viewport, float new_zoom);

Vert make_vert(float x, float y, float u, float v, Color c);
void vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert);

Render_Font load_font(const char *path, float dpi_scale);
float get_font_line_height(Render_Font font);
float get_char_width(char c, Render_Font font);
Rect get_string_rect(const char *str, Render_Font font, float x, float y);
Rect get_string_range_rect(const char *str, Render_Font font, int start_char, int end_char);
Rect get_string_char_rect(const char *str, Render_Font font, int char_i);
int get_char_i_at_pos_in_string(const char *str, Render_Font font, float x);
Rect get_cursor_rect(Text_Buffer text_buffer, Cursor_Pos cursor_pos, const Render_State *render_state);

Rect canvas_rect_to_screen_rect(Rect canvas_rect, Viewport canvas_viewport);
Vec_2 canvas_pos_to_screen_pos(Vec_2 canvas_pos, Viewport canvas_viewport);
Vec_2 screen_pos_to_canvas_pos(Vec_2 screen_pos, Viewport canvas_viewport);
Cursor_Pos buffer_pos_to_cursor_pos(Vec_2 buffer_pos, Text_Buffer text_buffer, const Render_State *render_state);
void viewport_snap_to_cursor(Text_Buffer text_buffer, Cursor_Pos cursor_pos, Viewport *viewport, Render_State *render_state);

void text_buffer_history_insert_char(Text_Buffer *text_buffer, History *history, char c, Cursor_Pos pos);
void text_buffer_history_remove_char(Text_Buffer *text_buffer, History *history, Cursor_Pos pos);
Cursor_Pos text_buffer_history_insert_range(Text_Buffer *text_buffer, History *history, const char *range, Cursor_Pos pos);
void text_buffer_history_remove_range(Text_Buffer *text_buffer, History *history, Cursor_Pos start, Cursor_Pos end);
int text_buffer_history_line_indent_increase_level(Text_Buffer *text_buffer, History *history, int line);
int text_buffer_history_line_indent_decrease_level(Text_Buffer *text_buffer, History *history, int line);
int text_buffer_history_line_indent_set_level(Text_Buffer *text_buffer, History *history, int line, int indent_level);
int text_buffer_history_line_match_indent(Text_Buffer *text_buffer, History *history, int line);
int text_buffer_history_whitespace_cleanup(Text_Buffer *text_buffer, History *history);

// ------------------------------

void buffer_view_set_mark(Buffer_View *buffer_view, Cursor_Pos pos);
void buffer_view_validate_mark(Buffer_View *buffer_view);
void buffer_view_set_cursor_to_pixel_position(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, const Render_State *render_state);

// --------------------------------

bool text_buffer_read_from_file(const char *path, Text_Buffer *text_buffer);
void text_buffer_write_to_file(Text_Buffer text_buffer, const char *path);

Image file_open_image(const char *path);

void read_clipboard_mac(char *buf, size_t buf_size);
void write_clipboard_mac(const char *text);

bool file_is_image(const char *path);
File_Kind file_detect_kind(const char *path);

char *sys_get_working_dir();
bool sys_change_working_dir(const char *dir, Editor_State *state);
bool sys_file_exists(const char *path);

// -------------------------------

Rect_Bounds rect_get_bounds(Rect r);
bool rect_intersect(Rect a, Rect b);
bool rect_p_intersect(Vec_2 p, Rect rect);
