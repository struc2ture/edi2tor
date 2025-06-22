#pragma once

#include "common.h"

#include <stdarg.h>
#include <stdbool.h>
#include <time.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include <stb_truetype.h>

#include "platform_types.h"
#include "scene_loader.h"
#include "draw.h"

#define SCROLL_SENS 10.0f
#define SCROLL_TIMEOUT 0.1f
#define VIEWPORT_CURSOR_BOUNDARY_LINES 5
#define VIEWPORT_CURSOR_BOUNDARY_COLUMNS 5
#define GO_TO_LINE_CHAR_MAX 32
#define MAX_LINES 16384
#define MAX_CHARS_PER_LINE 1024
#define INDENT_SPACES 4
#define DEFAULT_ZOOM 1.0f
#define FONT_PATH "res/UbuntuSansMono-Regular.ttf"
#define FONT_SIZE 18.0f
#define ENABLE_OS_CLIPBOARD true

#define FILE_PATH1 "res/mock6.txt"
// #define FILE_PATH1 "res/mock4.txt"
#define FILE_PATH2 "src/editor.c"
#define IMAGE_PATH "res/DUCKS.png"
#define LIVE_CUBE_PATH "bin/live_cube.dylib"

typedef struct {
    Rect rect;
    float zoom;
} Viewport;

typedef struct {
    char *path;
    bool has_been_modified;
} File_Info;

typedef struct {
    char *str;
    int len;
    int buf_len;
} Text_Line;

typedef struct {
    Text_Line *lines;
    int line_count;
} Text_Buffer;

typedef struct {
    int line;
    int col;
} Cursor_Pos;

typedef struct {
    const Text_Buffer *buf;
    Cursor_Pos pos;
} Cursor_Iterator;

typedef struct {
    Cursor_Pos pos;
    float blink_time;
} Display_Cursor;

typedef struct {
    Cursor_Pos start;
    Cursor_Pos end;
} Text_Selection;

typedef enum {
    PROMPT_OPEN_FILE,
    PROMPT_SAVE_AS,
    PROMPT_GO_TO_LINE,
    PROMPT_SEARCH_NEXT,
    PROMPT_CHANGE_WORKING_DIR
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

typedef enum {
    BUFFER_KIND_GENERIC,
    BUFFER_KIND_FILE,
    BUFFER_KIND_PROMPT
} Buffer_Kind;

typedef struct Live_Scene Live_Scene;

typedef struct {
    Buffer_Kind kind;
    Text_Buffer text_buffer;
    union {
    struct { /* nothing for generic */ } generic;
    struct {
        File_Info info;
        Live_Scene *linked_live_scene;
    } file;
    struct {
        Prompt_Context context;
    } prompt;
    };
} Buffer;

typedef struct {
    bool active;
    Cursor_Pos pos;
} Text_Mark;

struct Buffer_View {
    Buffer *buffer;
    Viewport viewport;
    Display_Cursor cursor;
    Text_Selection selection;
    Text_Mark mark;
    bool is_mouse_drag;
};

typedef struct {
    Image image;
    Rect image_rect;
} Image_View;

struct Live_Scene {
    void *state;
    Scene_Dylib dylib;
} ;

typedef struct {
    Live_Scene *live_scene;
    Framebuffer framebuffer;
    Rect framebuffer_rect;
} Live_Scene_View;

typedef enum  {
    VIEW_KIND_BUFFER,
    VIEW_KIND_IMAGE,
    VIEW_KIND_LIVE_SCENE
} View_Kind;

typedef struct {
    union {
        Buffer_View bv;
        Image_View iv;
        Live_Scene_View lsv;
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
} Mouse_State;

typedef struct {
    Render_State render_state;
    Mouse_State mouse_state;

    Buffer **buffers;
    int buffer_count;
    View **views;
    int view_count;
    View *active_view;

    Viewport canvas_viewport;

    char *copy_buffer;
    bool should_break;

    char *working_dir;

    char *prev_search;

    GLFWwindow *window;

    bool is_live_scene;
} Editor_State;

typedef enum {
    CURSOR_MOVE_LEFT,
    CURSOR_MOVE_RIGHT,
    CURSOR_MOVE_UP,
    CURSOR_MOVE_DOWN
} Cursor_Movement_Dir;

typedef struct {
    char **chunks;
    int chunk_count;
} String_Builder;

typedef enum {
    FILE_KIND_NONE,
    FILE_KIND_TEXT,
    FILE_KIND_IMAGE,
    FILE_KIND_DYLIB
} File_Kind;

void on_init(Editor_State *state, GLFWwindow *window, float window_w, float window_h, float window_px_w, float window_px_h, bool is_live_scene, GLuint fbo);
void on_reload(Editor_State *state);
void on_render(Editor_State *state, const Platform_Timing *t);
void on_platform_event(Editor_State *state, const Platform_Event *event);
void on_destroy(Editor_State *state);

bool gl_check_compile_success(GLuint shader, const char *src);
bool gl_check_link_success(GLuint prog);
GLuint gl_create_shader_program(const char *vs_src, const char *fs_src);
void gl_enable_scissor(Rect screen_rect, Render_State *render_state);
Framebuffer gl_create_framebuffer(int width, int height);

void initialize_render_state(Render_State *render_state, float window_w, float window_h, float window_px_w, float window_px_h, GLuint fbo);

Buffer **buffer_create_new_slot(Editor_State *state);
void buffer_free_slot(Buffer *buffer, Editor_State *state);
Buffer *buffer_create_generic(Text_Buffer text_buffer, Editor_State *state);
Buffer *buffer_create_read_file(const char *path, Editor_State *state);
Buffer *buffer_create_empty_file(Editor_State *state);
Buffer *buffer_create_prompt(const char *prompt_text, Prompt_Context context, Editor_State *state);
int buffer_get_index(Buffer *buffer, Editor_State *state);
void buffer_destroy(Buffer *buffer, Editor_State *state);

View *create_buffer_view_generic(Text_Buffer text_buffer, Rect rect, Editor_State *state);
View *create_buffer_view_open_file(const char *file_path, Rect rect, Editor_State *state);
View *create_buffer_view_empty_file(Rect rect, Editor_State *state);
View *create_buffer_view_prompt(const char *prompt_text, Prompt_Context context, Rect rect, Editor_State *state);
View *create_image_view(const char *file_path, Rect rect, Editor_State *state);
View *create_live_scene_view(const char *dylib_path, Rect rect, Editor_State *state);

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

Buffer_View *buffer_view_create(Buffer *buffer, Rect rect, Editor_State *state);
Rect buffer_view_get_text_area_rect(Buffer_View *buffer_view, const Render_State *render_state);
Rect buffer_view_get_line_num_col_rect(Buffer_View *buffer_view, const Render_State *render_state);
Rect buffer_view_get_name_rect(Buffer_View *buffer_view, const Render_State *render_state);
Vec_2 buffer_view_canvas_pos_to_text_area_pos(Buffer_View *buffer_view, Vec_2 canvas_pos, const Render_State *render_state);
Vec_2 buffer_view_text_area_pos_to_buffer_pos(Buffer_View *buffer_view, Vec_2 text_area_pos);

void image_destroy(Image image);

Image_View *image_view_create(Image image, Rect rect, Editor_State *state);

void framebuffer_destroy(Framebuffer framebuffer);

Live_Scene *live_scene_create(Editor_State *state, const char *path, float w, float h, GLuint fbo);
void live_scene_destroy(Live_Scene *render_scene);

Live_Scene_View *live_scene_view_create(Framebuffer framebuffer, Live_Scene *live_scene, Rect rect, Editor_State *state);

Prompt_Context prompt_create_context_open_file();
Prompt_Context prompt_create_context_go_to_line(Buffer_View *for_buffer_view);
Prompt_Context prompt_create_context_search_next(Buffer_View *for_buffer_view);
Prompt_Context prompt_create_context_save_as(Buffer_View *for_buffer_view);
Prompt_Context prompt_create_context_change_working_dir();
Prompt_Result prompt_parse_result(Text_Buffer text_buffer);
bool prompt_submit(Prompt_Context context, Prompt_Result result, Rect prompt_rect, Editor_State *state);

void viewport_set_outer_rect(Viewport *viewport, Rect outer_rect);
void viewport_set_zoom(Viewport *viewport, float new_zoom);

Rect get_cursor_rect(Text_Buffer text_buffer, Cursor_Pos cursor_pos, Render_State *render_state);

void draw_view(View *view, bool is_active, Viewport canvas_viewport, Render_State *render_state, const Platform_Timing *t);
void draw_buffer_view(Buffer_View *buffer_view, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time);
void draw_text_buffer(Text_Buffer text_buffer, Viewport viewport, Render_State *render_state);
void draw_cursor(Text_Buffer text_buffer, Display_Cursor *cursor, Viewport viewport, Render_State *render_state, float delta_time);
void draw_buffer_view_selection(Buffer_View *buffer_view, Render_State *render_state);
void draw_buffer_view_line_numbers(Buffer_View *buffer_view, Viewport canvas_viewport, Render_State *render_state);
void draw_buffer_view_name(Buffer_View *buffer_view, bool is_active, Viewport canvas_viewport, Render_State *render_state);

void draw_image_view(Image_View *image_view, Render_State *render_state);

void draw_live_scene_view(Live_Scene_View *live_scene_view, Render_State *render_state, const Platform_Timing *t);

void draw_status_bar(Editor_State *state, Render_State *render_state, const Platform_Timing *t);

Mat_4 viewport_get_transform(Viewport viewport);
void transform_set_buffer_view_text_area(Buffer_View *buffer_view, Viewport canvas_viewport, Render_State *render_state);
void transform_set_buffer_view_line_num_col(Buffer_View *buffer_view, Viewport canvas_viewport, Render_State *render_state);
void transform_set_rect(Rect rect, Viewport canvas_viewport, Render_State *render_state);
void transform_set_canvas_space(Viewport canvas_viewport, Render_State *render_state);
void transform_set_screen_space(Render_State *render_state);

Rect canvas_rect_to_screen_rect(Rect canvas_rect, Viewport canvas_viewport);
Vec_2 canvas_pos_to_screen_pos(Vec_2 canvas_pos, Viewport canvas_viewport);
Vec_2 screen_pos_to_canvas_pos(Vec_2 screen_pos, Viewport canvas_viewport);
Cursor_Pos buffer_pos_to_cursor_pos(Vec_2 buffer_pos, Text_Buffer text_buffer, const Render_State *render_state);
void viewport_snap_to_cursor(Text_Buffer text_buffer, Cursor_Pos cursor_pos, Viewport *viewport, Render_State *render_state);

bool cursor_iterator_next(Cursor_Iterator *it);
bool cursor_iterator_prev(Cursor_Iterator *it);
char cursor_iterator_get_char(Cursor_Iterator it);

Cursor_Pos cursor_pos_clamp(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_advance_char(Text_Buffer text_buffer, Cursor_Pos pos, int dir, bool can_switch_lines);
Cursor_Pos cursor_pos_advance_char_n(Text_Buffer text_buffer, Cursor_Pos pos, int n, int dir, bool can_switch_lines);
Cursor_Pos cursor_pos_advance_line(Text_Buffer text_buffer, Cursor_Pos pos, int dir);
Cursor_Pos cursor_pos_to_start_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos);
Cursor_Pos cursor_pos_to_end_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos);
Cursor_Pos cursor_pos_to_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_indent_or_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_end_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_end_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_prev_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_prev_start_of_paragraph(Text_Buffer text_buffer, Cursor_Pos pos);

Text_Line text_line_make_dup(const char *line);
Text_Line text_line_make_dup_range(const char *str, int start, int count);
Text_Line text_line_make_va(const char *fmt, va_list args);
Text_Line text_line_make_f(const char *fmt, ...);
Text_Line text_line_copy(Text_Line source, int start, int end);
void text_line___resize(Text_Line *text_line, int new_size);
void text_line_insert_char(Text_Line *text_line, char c, int insert_index);
void text_line_remove_char(Text_Line *text_line, int remove_index);
void text_line_insert_range(Text_Line *text_line, const char *range, int insert_index, int insert_count);
void text_line_remove_range(Text_Line *text_line, int remove_index, int remove_count);
int text_line_indent_get_level(Text_Line text_line);
int text_line_indent_set_level(Text_Line *text_line, int indent_level);
int text_line_indent_level_increase(Text_Line *text_line);
int text_line_indent_level_decrease(Text_Line *text_line);

Text_Buffer text_buffer_create_from_lines(const char *first, ...);
void text_buffer_destroy(Text_Buffer *text_buffer);
void text_buffer_validate(Text_Buffer *text_buffer);
void text_buffer_append_line(Text_Buffer *text_buffer, Text_Line text_line);
void text_buffer_insert_line(Text_Buffer *text_buffer, Text_Line new_line, int insert_at);
void text_buffer_remove_line(Text_Buffer *text_buffer, int remove_at);
void text_buffer_append_f(Text_Buffer *text_buffer, const char *fmt, ...);
void text_buffer___split_line(Text_Buffer *text_buffer, Cursor_Pos pos);
void text_buffer_insert_char(Text_Buffer *text_buffer, char c, Cursor_Pos pos);
void text_buffer_remove_char(Text_Buffer *text_buffer, Cursor_Pos pos);
Cursor_Pos text_buffer_insert_range(Text_Buffer *text_buffer, const char *range, Cursor_Pos pos);
void text_buffer_remove_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end);
char *text_buffer_extract_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end);
int text_buffer_match_indent(Text_Buffer *text_buffer, int line);
int text_buffer_whitespace_cleanup(Text_Buffer *text_buffer);
bool text_buffer_search_next(Text_Buffer *text_buffer, const char *query, Cursor_Pos from, Cursor_Pos *out_pos);

// ------------------------------

void buffer_view_set_mark(Buffer_View *buffer_view, Cursor_Pos pos);
void buffer_view_validate_mark(Buffer_View *buffer_view);
void buffer_view_set_cursor_to_pixel_position(Buffer_View *buffer_view, Vec_2 mouse_canvas_pos, const Render_State *render_state);

// --------------------------------

void string_builder_append_f(String_Builder *string_builder, const char *fmt, ...);
void string_builder_append_str_range(String_Builder *string_builder, const char *str, int start, int count);
char *string_builder_compile_and_destroy(String_Builder *string_builder);

bool file_read_into_text_buffer(const char *path, Text_Buffer *text_buffer, File_Info *file_info);
void file_write(Text_Buffer text_buffer, const char *path);

Image file_open_image(const char *path);

void read_clipboard_mac(char *buf, size_t buf_size);
void write_clipboard_mac(const char *text);

void live_scene_reset(Editor_State *state, Live_Scene **live_scene, float w, float h, GLuint fbo);
void live_scene_rebuild(Live_Scene *live_scene);

bool file_is_image(const char *path);
File_Kind file_detect_kind(const char *path);

char *sys_get_working_dir();
bool sys_change_working_dir(const char *dir, Editor_State *state);
bool sys_file_exists(const char *path);
