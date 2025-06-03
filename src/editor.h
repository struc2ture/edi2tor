#pragma once

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

#define FILE_PATH1 "src/editor.c"
// #define FILE_PATH1 "res/mock4.txt"
#define FILE_PATH2 "res/mock5.txt"

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
    Rect screen_rect;
    Vec_2 canvas_offset;
    float canvas_zoom;
} Viewport;

typedef struct {
    char *path;
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
} Buf_Pos;

typedef struct {
    Buf_Pos pos;
    float blink_time;
} Text_Cursor;

typedef struct {
    Buf_Pos start;
    Buf_Pos end;
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
    GLuint prog;
    GLuint vao;
    GLuint vbo;
    GLuint shader_mvp_loc;
    Vec_2 window_dim;
    Vec_2 framebuffer_dim;
    float dpi_scale;
    Render_Font font;
    float line_num_col_width;
} Render_State;

typedef struct {
    File_Info file_info;
    Text_Buffer text_buffer;
    Rect outer_rect;
    Viewport viewport;
    Text_Cursor cursor;
    Text_Selection selection;
} Buffer_View;

typedef struct {
    Render_State render_state;
    Buffer_View **buffer_views;
    int buffer_view_count;
    Buffer_View *active_buffer_view;
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
    bool is_viewport_drag;
    bool is_viewport_resize;
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

void handle_key_input(GLFWwindow *window, Editor_State *state, int key, int action, int mods);
void handle_char_input(Editor_State *state, char c);
void handle_mouse_input(GLFWwindow *window, Editor_State *state);

Cursor_Movement_Dir get_cursor_movement_dir_by_key(int key);
void handle_cursor_movement_keys(Buffer_View *buffer_view, Cursor_Movement_Dir dir, bool with_selection, bool big_steps, bool start_end, Editor_State *state);
void handle_mouse_buffer_click(Buffer_View *buffer_view, bool with_selection, bool just_pressed, Vec_2 mouse_window_pos, Editor_State *state);

void initialize_render_state(GLFWwindow *window, Render_State *render_state);
void perform_timing_calculations(Editor_State *state);

Buffer_View *buffer_view_create(Rect rect, Render_State *render_state, Editor_State *state);
Buffer_View *buffer_view_open_file(const char *file_path, Rect rect, Render_State *render_state, Editor_State *state);
void buffer_view_set_rect(Buffer_View *buffer_view, Rect rect, Render_State *render_state);
int buffer_view_get_index(Buffer_View *buffer_view, Editor_State *state);
void buffer_view_set_active(Buffer_View *buffer_view, Editor_State *state);

Vert make_vert(float x, float y, float u, float v, const unsigned char color[4]);
void vert_buffer_add_vert(Vert_Buffer *vert_buffer, Vert vert);

Render_Font load_font(const char *path);
float get_font_line_height(Render_Font font);
float get_char_width(char c, Render_Font font);
Rect get_string_rect(const char *str, Render_Font font, float x, float y);
Rect get_string_range_rect(const char *str, Render_Font font, int start_char, int end_char);
Rect get_string_char_rect(const char *str, Render_Font font, int char_i);
int get_char_i_at_pos_in_string(const char *str, Render_Font font, float x);
Rect get_cursor_rect(Text_Buffer text_buffer, Text_Cursor cursor, Render_State *render_state);

void draw_string(const char *str, Render_Font font, float x, float y, const unsigned char color[4]);
void draw_quad(Rect q, const unsigned char color[4]);

void draw_text_buffer(Text_Buffer text_buffer, Viewport viewport, Render_State *render_state);
void draw_cursor(Text_Buffer text_buffer, Text_Cursor *cursor, Viewport viewport, Render_State *render_state, float delta_time);
void draw_selection(Text_Buffer text_buffer, Text_Selection selection, Viewport viewport, Render_State *render_state);
void draw_line_numbers(Text_Buffer text_buffer, Text_Cursor cursor, Viewport viewport, Render_State *render_state);
void draw_buffer_view(Buffer_View *buffer_view, bool is_active, Render_State *render_state, float delta_time);

void draw_status_bar(GLFWwindow *window, Editor_State *state, Render_State *render_state);

void make_ortho(float left, float right, float bottom, float top, float near, float far, float *out);
void make_view(float offset_x, float offset_y, float scale, float *out);
void make_mat4_identity(float *out);
void mul_mat4(const float *a, const float *b, float *out);
void gl_enable_scissor(Rect screen_rect, Render_State *render_state);
void set_viewport_transform(Viewport viewport, Render_State *render_state);
void set_line_num_col_transform(Viewport viewport, Render_State *render_state);
void set_screen_space_transform(Render_State *render_state);
void update_mvp_vertical_canvas_space(Render_State *render_state, Viewport viewport);

Rect_Bounds get_viewport_bounds(Viewport viewport);
Rect_Bounds get_viewport_cursor_bounds(Viewport viewport, Render_Font font);
Rect get_viewport_rect(Viewport viewport);
Vec_2 window_pos_to_canvas_pos(Vec_2 window_pos, Viewport viewport);
bool is_canvas_pos_in_bounds(Vec_2 canvas_pos, Viewport viewport);
bool is_pos_in_bounds(Vec_2 pos, Rect_Bounds bounds);
bool is_canvas_y_pos_in_bounds(float canvas_y, Viewport viewport);
Vec_2 get_mouse_window_pos(GLFWwindow *window);
Vec_2 get_mouse_delta(GLFWwindow *window, Editor_State *state);
Buffer_View *get_buffer_view_at_pos(Vec_2 pos, Editor_State *state);
Buf_Pos canvas_pos_to_buf_pos(Vec_2 canvas_pos, const Buffer_View *buffer_view, const Render_State *render_state);
void viewport_snap_to_cursor(Text_Buffer text_buffer, Text_Cursor cursor, Viewport *viewport, Render_State *render_state);
bool is_canvas_rect_in_viewport(Viewport viewport, Rect rect);

bool is_buf_pos_valid(Text_Buffer tb, Buf_Pos bp);
bool is_buf_pos_equal(Buf_Pos a, Buf_Pos b);
void cancel_selection(Editor_State *state);

void move_cursor_to_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state, int to_line, bool snap_viewport);
void move_cursor_to_col(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state, int to_char, bool snap_viewport, bool can_switch_line);

void move_cursor_to_next_end_of_word(Editor_State *state);
void move_cursor_to_prev_start_of_word(Editor_State *state);
void move_cursor_to_next_white_line(Editor_State *state);
void move_cursor_to_prev_white_line(Editor_State *state);

Text_Line make_text_line_dup(char *line);
Text_Line copy_text_line(Text_Line source, int start, int end);
void resize_text_line(Text_Line *text_line, int new_size);
void insert_line(Text_Buffer *text_buffer, Text_Line new_line, int insert_at);
void remove_line(Text_Buffer *text_buffer, int remove_at);

void insert_char(Text_Buffer *text_buffer, char c, Text_Cursor *cursor, Editor_State *state, bool auto_indent);
void remove_char(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state);
void insert_indent(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state);
int get_line_indent(Text_Line line);
void decrease_indent_level_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state);
void decrease_indent_level(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state);
void increase_indent_level_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state);
void increase_indent_level(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state);
void delete_current_line(Editor_State *state);
bool is_white_line(Text_Line line);

void open_file_for_edit(const char *path, Buffer_View *buffer_view, Editor_State *state);
File_Info read_file(const char *path, Text_Buffer *text_buffer);
void write_file(Text_Buffer text_buffer, File_Info file_info);

void start_selection_at_cursor(Editor_State *state);
void extend_selection_to_cursor(Editor_State *state);
void cancel_selection(Editor_State *state);
bool is_selection_valid(Text_Buffer text_buffer, Text_Selection selection);
int selection_char_count(Editor_State *state);
void delete_selected(Editor_State *state);

void copy_at_selection(Editor_State *state);
void paste_from_copy_buffer(Editor_State *state);

void validate_text_buffer(Text_Buffer *text_buffer);

void rebuild_dl();
void insert_go_to_line_char(Editor_State *state, char c);
