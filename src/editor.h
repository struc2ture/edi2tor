#pragma once

#include <stdbool.h>

#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#define VERT_MAX 4096
#define CURSOR_BLINK_ON_FRAMES 30
#define SCROLL_SENS 10.0f
#define LINE_HEIGHT 15
#define VIEWPORT_CURSOR_BOUNDARY_LINES 5
#define ENABLE_STATUS_BAR true
#define GO_TO_LINE_CHAR_MAX 32
#define MAX_LINES 16384
#define MAX_CHARS_PER_LINE 1024
#define INDENT_SPACES 4

#define FILE_PATH "src/editor.c"
// #define FILE_PATH "res/mock4.txt"

typedef struct {
    float x, y;
} Vec_2;
#define VEC2_FMT "<%0.2f, %0.2f>"
#define VEC2_ARG(v) (v).x, (v).y

typedef struct {
    Vec_2 min;
    Vec_2 max;
} Rect_Bounds;

typedef struct {
    float x, y;
    unsigned char r, g, b, a;
} Vert;

typedef struct {
    Vert verts[VERT_MAX];
    int vert_count;
} Vert_Buffer;

typedef struct {
    Vec_2 offset;
    Vec_2 window_dim;
    float zoom;
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
    int l;
    int c;
} Buf_Pos;

typedef struct {
    Buf_Pos pos;
    int frame_count;
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
    GLuint prog;
    GLuint vao;
    GLuint vbo;
    GLuint shader_mvp_loc;
    float view_transform[16];
    float proj_transform[16];
    Viewport viewport;
    File_Info file_info;
    Text_Buffer text_buffer;
    Text_Cursor cursor;
    Text_Selection selection;
    Copy_Buffer copy_buffer;
    bool should_break;
    bool debug_invis;
    long long frame_count;
    bool go_to_line_mode;
    char go_to_line_chars[GO_TO_LINE_CHAR_MAX];
    int current_go_to_line_char_i;
    bool left_mouse_down;
} Editor_State;

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

void render_string(int x, int y, char *line, unsigned char color[4], Vert_Buffer *out_vert_buf);
void draw_quad(int x, int y, int width, int height, unsigned char color[4]);
void draw_string(int x, int y, char *string, unsigned char color[4]);
void draw_content_string(int x, int y, char *string, unsigned char color[4], Editor_State *state);

void make_ortho(float left, float right, float bottom, float top, float near, float far, float *out);
void make_view(float offset_x, float offset_y, float scale, float *out);
void mul_mat4(const float *a, const float *b, float *out);

Rect_Bounds get_viewport_bounds(Viewport viewport);
Vec_2 get_viewport_dim(Viewport viewport);
Vec_2 window_pos_to_canvas_pos(Vec_2 window_pos, Viewport viewport);
bool is_canvas_pos_in_bounds(Vec_2 canvas_pos, Viewport viewport);
bool is_canvas_y_pos_in_bounds(float canvas_y, Viewport viewport);
Vec_2 get_mouse_canvas_pos(GLFWwindow *window, Viewport viewport);
Buf_Pos get_buf_pos_under_mouse(GLFWwindow *window, Editor_State *state);
void update_shader_mvp(Editor_State *state);
void viewport_snap_to_cursor(Text_Cursor *cursor, Viewport *viewport, Editor_State *state);

bool is_buf_pos_valid(const Text_Buffer *tb, Buf_Pos bp);
bool is_buf_pos_equal(Buf_Pos a, Buf_Pos b);
void cancel_selection(Editor_State *state);

void move_cursor_to_line(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state, int to_line, bool snap_viewport);
void move_cursor_to_char(Text_Buffer *text_buffer, Text_Cursor *cursor, Editor_State *state, int to_char, bool snap_viewport, bool can_switch_line);

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

void open_file_for_edit(const char *path, Editor_State *state);
File_Info read_file(const char *path, Text_Buffer *text_buffer);
void write_file(Text_Buffer text_buffer, File_Info file_info);

void start_selection_at_cursor(Editor_State *state);
void extend_selection_to_cursor(Editor_State *state);
void cancel_selection(Editor_State *state);
bool is_selection_valid(const Editor_State *state);
int selection_char_count(Editor_State *state);
void delete_selected(Editor_State *state);

void copy_at_selection(Editor_State *state);
void paste_from_copy_buffer(Editor_State *state);

void validate_text_buffer(Text_Buffer *text_buffer);

void rebuild_dl();
void insert_go_to_line_char(Editor_State *state, char c);
void convert_to_debug_invis(char *string);
