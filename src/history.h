#pragma once

#include "misc.h"
#include "util.h"

typedef enum {
    DELTA_INSERT_CHAR,
    DELTA_REMOVE_CHAR,
    DELTA_INSERT_RANGE,
    DELTA_REMOVE_RANGE
} DeltaKind;

typedef struct {
    union {
        struct {
            Cursor_Pos pos;
        } insert_char;

        struct {
            Cursor_Pos pos;
            char c;
        } remove_char;

        struct {
            Cursor_Pos pos;
            Cursor_Pos end;
        } insert_range;

        struct {
            Cursor_Pos start;
            char *range;
        } remove_range;
    };

    DeltaKind kind;
} Delta;

typedef struct {
    const char *name;
    Delta *deltas;
    int delta_count;
    int delta_cap;
    Cursor_Pos pos;
    bool committed;
} Command;

typedef struct {
    Command *commands;
    int command_count;
    int command_cap;
} History;

bool history_begin_command(History *history, Cursor_Pos pos, const char *command_name)
{
    if (history->command_count > 0 && !history->commands[history->command_count - 1].committed)
    {
        return false;
    }

    history->command_count++;

    bool need_realloc = false;
    if (history->command_cap == 0)
    {
        history->command_cap = 8;
        need_realloc = true;
    }
    if (history->command_count >= history->command_cap)
    {
        history->command_cap *= 2;
        need_realloc = true;
    }
    if (need_realloc)
    {
        history->commands = xrealloc(history->commands, history->command_cap * sizeof(history->commands[0]));
    }

    history->commands[history->command_count - 1] = (Command){0};
    history->commands[history->command_count - 1].name = xstrdup(command_name);
    history->commands[history->command_count - 1].pos = pos;

    return true;
}

void history_add_delta(History *history, const Delta *delta)
{
    Command *command = &history->commands[history->command_count - 1];

    bool need_realloc = false;
    if (command->delta_cap == 0)
    {
        command->delta_cap = 4;
        need_realloc = true;
    }
    if (command->delta_count >= command->delta_cap)
    {
        command->delta_cap *= 2;
        need_realloc = true;
    }
    if (need_realloc)
    {
        command->deltas = xrealloc(command->deltas, command->delta_cap * sizeof(command->deltas[0]));
    }

    command->deltas[command->delta_count++] = *delta;
}

void history_commit_command(History *history)
{
    history->commands[history->command_count - 1].committed = true;
}

#if 0
void usage()
{
    History history = {0};

    history_begin_command(&history, (Cursor_Pos){0, 0}, "outer command");

    history_add_delta(&history, &(Delta) {
        .kind = DELTA_REMOVE_RANGE,
        .remove_range = {
            .start = (Cursor_Pos){2, 1},
            .range = "abc"
        }
    });

    history_add_delta(&history, &(Delta) {
        .kind = DELTA_INSERT_CHAR,
        .insert_char = {
            .pos = (Cursor_Pos){2, 1}
        }
    });

    {
        bool new_command = history_begin_command(&history, (Cursor_Pos){0, 0}, "inner command");
        history_add_delta(&history, &(Delta) {
            .kind = DELTA_REMOVE_RANGE,
            .remove_range = {
                .start = (Cursor_Pos){2, 1},
                .range = "cde"
            }
        });
        if (new_command) history_commit_command(&history);
    }

    history_commit_command(&history);
}
#endif
