#pragma once

#include "text_buffer.h"

typedef enum {
    DELTA_INSERT_CHAR,
    DELTA_REMOVE_CHAR,
    DELTA_INSERT_RANGE,
    DELTA_REMOVE_RANGE
} DeltaKind;

static const char *DeltaKind_Str[] = { "Insert char", "Remove char", "Insert range", "Remove range" };

typedef struct {
    union {
        struct {
            Cursor_Pos pos;
            char c;
        } insert_char;

        struct {
            Cursor_Pos pos;
            char c;
        } remove_char;

        struct {
            Cursor_Pos start;
            Cursor_Pos end;
            char *range;
        } insert_range;

        struct {
            Cursor_Pos start;
            Cursor_Pos end;
            char *range;
        } remove_range;
    };

    DeltaKind kind;
} Delta;

typedef enum {
    RUNNING_COMMAND_NONE,
    RUNNING_COMMAND_TEXT_INSERT,
    RUNNING_COMMAND_TEXT_DELETION,
} Running_Command_Kind;

typedef struct {
    const char *name;
    Delta *deltas;
    int delta_count;
    int delta_cap;
    Cursor_Pos cursor_pos;
    Text_Mark mark;
    Running_Command_Kind running_command_kind;
    bool committed;
} Command;

typedef struct {
    Command *commands;
    int command_count;
    int command_cap;
    int history_pos;
} History;

bool history_begin_command_0(History *history,
    Cursor_Pos cursor_pos,
    Text_Mark mark,
    const char *command_name,
    Running_Command_Kind running_command_kind,
    bool can_interrupt,
    bool reset_history);

bool history_begin_command_running(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name, Running_Command_Kind running_kind);
bool history_begin_command_non_interrupt(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name);
bool history_begin_command_non_reset(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name);
bool history_begin_command(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name);
void history_add_delta(History *history, const Delta *delta);
void history_commit_command(History *history);
Delta *history_get_last_delta(History *history);
Command *history_get_last_uncommitted_command(History *history);
Command *history_get_command_to_undo(History *history);
