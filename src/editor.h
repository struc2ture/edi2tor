#pragma once

#include <stdarg.h>
#include <stdbool.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#define VERT_MAX 4096
#define SCROLL_SENS 10.0f
#define VIEWPORT_CURSOR_BOUNDARY_LINES 5
#define VIEWPORT_CURSOR_BOUNDARY_COLUMNS 5
#define GO_TO_LINE_CHAR_MAX 32
#define MAX_LINES 16384
#define MAX_CHARS_PER_LINE 1024
#define INDENT_SPACES 4
#define DEFAULT_ZOOM 1.0f
#define FONT_PATH "res/UbuntuSansMono-Regular.ttf"
#define FONT_SIZE 20.0f

#define FILE_PATH1 "res/mock6.txt"
// #define FILE_PATH1 "res/mock4.txt"
#define FILE_PATH2 "src/editor.c"

typedef struct {
    float x, y;
} Vec_2;
#define VEC2_FMT "<%0.2f, %0.2f>"
#define VEC2_ARG(v) (v).x, (v).y

typedef struct {
    float x, y;
    float w, h;
} Rect;

typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} Rect_Bounds;

typedef struct {
    float x, y;
    float u, v;
    unsigned char r, g, b, a;
} Vert;

typedef struct {
    Vert verts[VERT_MAX];
    int vert_count;
} Vert_Buffer;

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

typedef struct {
    Text_Line *lines;
    int line_count;
} Copy_Buffer;

typedef struct {
    stbtt_bakedchar *char_data;
    int char_count;
    GLuint texture;
    GLuint white_texture;
    float size;
    float ascent, descent, line_gap;
    int atlas_w, atlas_h;
} Render_Font;

typedef struct {
    GLuint main_shader;
    GLuint grid_shader;
    GLuint vao;
    GLuint vbo;
    GLuint main_shader_mvp_loc;
    GLuint grid_shader_mvp_loc;
    GLuint grid_shader_offset_loc;
    GLuint grid_shader_resolution_loc;
    Vec_2 window_dim;
    Vec_2 framebuffer_dim;
    float dpi_scale;
    Render_Font font;
    float buffer_view_line_num_col_width;
    float buffer_view_name_height;
    float buffer_view_padding;
    float buffer_view_resize_handle_radius;
} Render_State;

typedef enum {
    PROMPT_OPEN_FILE,
    PROMPT_SAVE_AS,
    PROMPT_GO_TO_LINE
} Prompt_Kind;

typedef struct View View;
typedef struct {
    Prompt_Kind kind;
    union {
    struct {/* nothing for open_file */} open_file;
    struct {
        View *for_view;
    } go_to_line;
    struct {
        View *for_view;
    } save_as;
    };
} Prompt_Context;

typedef struct {
    char str[MAX_CHARS_PER_LINE];
} Prompt_Result;

typedef enum {
    BUFFER_GENERIC,
    BUFFER_FILE,
    BUFFER_PROMPT
} Buffer_Kind;

typedef struct {
    Buffer_Kind kind;
    Text_Buffer text_buffer;
    union {
    struct { /* nothing for generic */ } generic;
    struct {
        File_Info info;
    } file;
    struct {
        Prompt_Context context;
    } prompt;
    };
} Buffer;

typedef enum  {
    VIEW_BUFFER
} View_Kind;

struct View {
    View_Kind kind;
    Rect outer_rect;
    union {
    struct {
        Buffer *buffer;
        Viewport viewport;
        Display_Cursor cursor;
        Text_Selection selection;
    } buffer;
    };
};

typedef struct {
    Render_State render_state;
    Buffer **buffers;
    int buffer_count;
    View **views;
    int view_count;
    View *active_view;
    Viewport canvas_viewport;
    Copy_Buffer copy_buffer;
    bool should_break;
    float delta_time;
    float last_frame_time;
    float last_fps_time;
    int fps_frame_count;
    float fps;
    long long frame_count;
    bool left_mouse_down;
    bool left_mouse_handled;
    Vec_2 prev_mouse_pos;
    bool is_buffer_view_drag;
    bool is_buffer_view_resize;
    bool is_buffer_view_text_area_click;
    float scroll_timeout;
    View *scrolled_buffer_view;
} Editor_State;

typedef enum {
    CURSOR_MOVE_LEFT,
    CURSOR_MOVE_RIGHT,
    CURSOR_MOVE_UP,
    CURSOR_MOVE_DOWN
} Cursor_Movement_Dir;

void _init(GLFWwindow *window, void *_state);
void _hotreload_init(GLFWwindow *window);
void _render(GLFWwindow *window, void *_state);

void char_callback(GLFWwindow *window, unsigned int codepoint);
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void scroll_callback(GLFWwindow *window, double x_offset, double y_offset);
void framebuffer_size_callback(GLFWwindow *window, int w, int h);
void window_size_callback(GLFWwindow *window, int w, int h);
void refresh_callback(GLFWwindow *window);

bool gl_check_compile_success(GLuint shader, const char *src);
bool gl_check_link_success(GLuint prog);
GLuint gl_create_shader_program(const char *vs_src, const char *fs_src);
void gl_enable_scissor(Rect screen_rect, Render_State *render_state);

void initialize_render_state(GLFWwindow *window, Render_State *render_state);
void perform_timing_calculations(Editor_State *state);

Buffer **buffer_create_new_slot(Editor_State *state);
void buffer_free_slot(Buffer *buffer, Editor_State *state);
Buffer *buffer_create_generic(Text_Buffer text_buffer, Editor_State *state);
Buffer *buffer_create_read_file(const char *path, Editor_State *state);
Buffer *buffer_create_prompt(const char *prompt_text, Prompt_Context context, Editor_State *state);
int buffer_get_index(Buffer *buffer, Editor_State *state);
void buffer_destroy(Buffer *buffer, Editor_State *state);

int view_get_index(View *view, Editor_State *state);
bool view_exists(View *view, Editor_State *state);
void view_set_active(View *view, Editor_State *state);
Rect view_get_resize_handle_rect(View view, const Render_State *render_state);
void view_set_rect(View *view, Rect rect, const Render_State *render_state);
View *view_at_pos(Vec_2 pos, Editor_State *state);

View *view_buffer_create(Buffer *buffer, Rect rect, Editor_State *state);
View *view_buffer_generic(Text_Buffer text_buffer, Rect rect, Editor_State *state);
View *view_buffer_open_file(const char *file_path, Rect rect, Editor_State *state);
View *view_buffer_prompt(const char *prompt_text, Prompt_Context context, Rect rect, Editor_State *state);
void view_buffer_destroy(View *view, Editor_State *state);
Rect view_buffer_get_text_area_rect(View view, const Render_State *render_state);
Rect view_buffer_get_line_num_col_rect(View view, const Render_State *render_state);
Rect view_buffer_get_name_rect(View view, const Render_State *render_state);
Vec_2 view_buffer_canvas_pos_to_text_area_pos(View view, Vec_2 canvas_pos, const Render_State *render_state);
Vec_2 view_buffer_text_area_pos_to_buffer_pos(View view, Vec_2 text_area_pos);

Prompt_Context prompt_create_context_open_file();
Prompt_Context prompt_create_context_go_to_line(View *for_view);
Prompt_Context prompt_create_context_save_as(View *for_view);
Prompt_Result prompt_parse_result(Text_Buffer text_buffer);
void prompt_submit(Prompt_Context context, Prompt_Result result, Rect prompt_rect, GLFWwindow *window, Editor_State *state);

void viewport_set_outer_rect(Viewport *viewport, Rect outer_rect);
void viewport_set_zoom(Viewport *viewport, float new_zoom);

Vert make_vert(float x, float y, float u, float v, const unsigned char color[4]);
void vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert);

Render_Font load_font(const char *path);
float get_font_line_height(Render_Font font);
float get_char_width(char c, Render_Font font);
Rect get_string_rect(const char *str, Render_Font font, float x, float y);
Rect get_string_range_rect(const char *str, Render_Font font, int start_char, int end_char);
Rect get_string_char_rect(const char *str, Render_Font font, int char_i);
int get_char_i_at_pos_in_string(const char *str, Render_Font font, float x);
Rect get_cursor_rect(Text_Buffer text_buffer, Display_Cursor cursor, Render_State *render_state);

void draw_string(const char *str, Render_Font font, float x, float y, const unsigned char color[4]);
void draw_quad(Rect q, const unsigned char color[4]);

void draw_grid(Viewport canvas_viewport, Render_State *render_state);

void draw_text_buffer(Text_Buffer text_buffer, Viewport viewport, Render_State *render_state);
void draw_cursor(Text_Buffer text_buffer, Display_Cursor *cursor, Viewport viewport, Render_State *render_state, float delta_time);
void draw_selection(Text_Buffer text_buffer, Text_Selection selection, Viewport viewport, Render_State *render_state);
void draw_view_buffer_line_numbers(View view, Viewport canvas_viewport, Render_State *render_state);
void draw_view_buffer_name(View view, bool is_active, Viewport canvas_viewport, Render_State *render_state);
void draw_view_buffer(View view, bool is_active, Viewport canvas_viewport, Render_State *render_state, float delta_time);

void draw_status_bar(GLFWwindow *window, Editor_State *state, Render_State *render_state);

void make_ortho(float left, float right, float bottom, float top, float near, float far, float *out);
void make_view(float offset_x, float offset_y, float scale, float *out);
void make_viewport_transform(Viewport viewport, float *out);
void make_mat4_identity(float *out);
void mul_mat4(const float *a, const float *b, float *out);
void transform_set_view_buffer_text_area(View view, Viewport canvas_viewport, Render_State *render_state);
void transform_set_view_buffer_line_num_col(View view, Viewport canvas_viewport, Render_State *render_state);
void transform_set_rect(Rect rect, Viewport canvas_viewport, Render_State *render_state);
void transform_set_canvas_space(Viewport canvas_viewport, Render_State *render_state);
void transform_set_screen_space(Render_State *render_state);

Rect canvas_rect_to_screen_rect(Rect canvas_rect, Viewport canvas_viewport);
Vec_2 screen_pos_to_canvas_pos(Vec_2 screen_pos, Viewport canvas_viewport);
Vec_2 get_mouse_screen_pos(GLFWwindow *window);
Vec_2 get_mouse_canvas_pos(GLFWwindow *window, Editor_State *state);
Vec_2 get_mouse_delta(GLFWwindow *window, Editor_State *state);
Cursor_Pos buffer_pos_to_cursor_pos(Vec_2 buffer_pos, Text_Buffer text_buffer, const Render_State *render_state);
void viewport_snap_to_cursor(Text_Buffer text_buffer, Display_Cursor cursor, Viewport *viewport, Render_State *render_state);

bool is_cursor_pos_valid(Text_Buffer tb, Cursor_Pos bp);
bool is_cursor_pos_equal(Cursor_Pos a, Cursor_Pos b);
void cancel_selection(Editor_State *state);

bool cursor_iterator_next(Cursor_Iterator *it);
bool cursor_iterator_prev(Cursor_Iterator *it);
char cursor_iterator_get_char(Cursor_Iterator it);

Cursor_Pos cursor_pos_clamp(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_advance_char(Text_Buffer text_buffer, Cursor_Pos pos, int dir, bool can_switch_lines);
Cursor_Pos cursor_pos_advance_line(Text_Buffer text_buffer, Cursor_Pos pos, int dir);
Cursor_Pos cursor_pos_to_start_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos);
Cursor_Pos cursor_pos_to_end_of_buffer(Text_Buffer text_buffer, Cursor_Pos cursor_pos);
Cursor_Pos cursor_pos_to_start_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_end_of_line(Text_Buffer text_buffer, Cursor_Pos pos);
Cursor_Pos cursor_pos_to_next_start_of_word(Text_Buffer text_buffer, Cursor_Pos pos);
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
void text_buffer_insert_range(Text_Buffer *text_buffer, const char *range, Cursor_Pos pos);
void text_buffer_remove_range(Text_Buffer *text_buffer, Cursor_Pos start, Cursor_Pos end);

void insert_char(Text_Buffer *text_buffer, char c, Display_Cursor *cursor, Editor_State *state, bool auto_indent);
void remove_char(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state);
void insert_indent(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state);
int get_line_indent(Text_Line line);
void decrease_indent_level_line(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state);
void decrease_indent_level(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state);
void increase_indent_level_line(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state);
void increase_indent_level(Text_Buffer *text_buffer, Display_Cursor *cursor, Editor_State *state);
void delete_current_line(Editor_State *state);
bool is_white_line(Text_Line line);

File_Info file_read_into_text_buffer(const char *path, Text_Buffer *text_buffer);
void file_write(Text_Buffer text_buffer, const char *path);

void start_selection_at_cursor(Editor_State *state);
void extend_selection_to_cursor(Editor_State *state);
void cancel_selection(Editor_State *state);
bool is_selection_valid(Text_Buffer text_buffer, Text_Selection selection);
int selection_char_count(Editor_State *state);
void delete_selected(Editor_State *state);

void copy_at_selection(Editor_State *state);
void paste_from_copy_buffer(Editor_State *state);

void rebuild_dl();
void insert_go_to_line_char(Editor_State *state, char c);

void handle_key_input(GLFWwindow *window, Editor_State *state, int key, int action, int mods);
void handle_char_input(Editor_State *state, char c);
void handle_mouse_input(GLFWwindow *window, Editor_State *state);

Cursor_Movement_Dir get_cursor_movement_dir_by_key(int key);
void handle_cursor_movement_keys(View *view, Cursor_Movement_Dir dir, bool with_selection, bool big_steps, bool start_end, Editor_State *state);
void handle_mouse_text_area_click(View *view, bool with_selection, bool just_pressed, Vec_2 mouse_screen_pos, Editor_State *state);
