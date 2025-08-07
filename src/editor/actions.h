#pragma once

#include "editor.h"

bool action_run_unit_tests(Editor_State *state);
bool action_change_working_dir(Editor_State *state);
bool action_destroy_active_view(Editor_State *state);
bool action_open_test_file1(Editor_State *state);
bool action_open_test_image(Editor_State *state);
bool action_prompt_open_file(Editor_State *state);
bool action_prompt_new_file(Editor_State *state);
bool action_save_workspace(Editor_State *state);
bool action_load_workspace(Editor_State *state);
bool action_reload_workspace(Editor_State *state);
bool action_temp_load_debug_scene(Editor_State *state);
bool action_temp_load_cube_scene(Editor_State *state);

bool action_buffer_view_move_cursor(Editor_State *state, Buffer_View *buffer_view, Cursor_Movement_Dir dir, bool with_shift, bool with_alt, bool with_super);
bool action_buffer_view_prompt_submit(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_input_char(Editor_State *state, Buffer_View *buffer_view, char c);
bool action_buffer_view_delete_selected(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_backspace(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_backspace_word(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_insert_indent(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_decrease_indent_level(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_increase_indent_level(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_copy_selected(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_cut_selected(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_paste(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_delete_current_line(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_reload_file(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_prompt_save_file_as(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_save_file(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_change_zoom(Editor_State *state, Buffer_View *buffer_view, float amount);
bool action_buffer_view_prompt_go_to_line(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_prompt_search_next(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_repeat_search(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_whitespace_cleanup(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_view_history(Editor_State *state, Buffer_View *buffer_view);
bool action_buffer_view_undo_command(Editor_State *state, Buffer_View *buffer_view);
bool action_run_scratch_for_buffer(Editor_State *state, Buffer *buffer);
