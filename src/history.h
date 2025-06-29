#pragma once

#include "misc.h"
#include "util.h"

typedef enum {
    DELTA_INSERT_CHAR,
    DELTA_REMOVE_CHAR,
    DELTA_INSERT_RANGE,
    DELTA_REMOVE_RANGE
} DeltaKind;

const char *DeltaKind_Str[] = { "Insert char", "Remove char", "Insert range", "Remove range" };

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
    Cursor_Pos pos;
    Running_Command_Kind running_command_kind;
    bool committed;
} Command;

typedef struct {
    Command *commands;
    int command_count;
    int command_cap;
    int history_pos;
} History;

void history_commit_command(History *history);

bool history_begin_command_0(History *history, Cursor_Pos pos, const char *command_name, Running_Command_Kind running_command_kind, bool can_interrupt, bool reset_history)
{
    if (history->command_count > 0)
    {
        Command *c = &history->commands[history->command_count - 1];
        if (!c->committed)
        {
            if (c->running_command_kind != RUNNING_COMMAND_NONE &&
                c->running_command_kind != running_command_kind &&
                can_interrupt)
            {
                // There's a running command already, and a command that is non-running or of a different kind is getting started
                // Commit the previous running command, and begin a new one
                history_commit_command(history);
            }
            else
            {
                // If a previous command has not been committed, do not begin a new one (with the exception of above)
                return false;
            }
        }
    }

    history->command_count++;
    if (reset_history)
    {
        history->history_pos = history->command_count;
    }

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
    history->commands[history->command_count - 1].running_command_kind = running_command_kind;

    return true;
}

bool history_begin_command_running(History *history, Cursor_Pos pos, const char *command_name, Running_Command_Kind running_kind)
{
    return history_begin_command_0(history, pos, command_name, running_kind, true, true);
}

bool history_begin_command_non_interrupt(History *history, Cursor_Pos pos, const char *command_name)
{
    return history_begin_command_0(history, pos, command_name, RUNNING_COMMAND_NONE, false, true);
}

bool history_begin_command(History *history, Cursor_Pos pos, const char *command_name)
{
    return history_begin_command_0(history, pos, command_name, RUNNING_COMMAND_NONE, true, true);
}

bool history_begin_command_non_reset(History *history, Cursor_Pos pos, const char *command_name)
{
    return history_begin_command_0(history, pos, command_name, RUNNING_COMMAND_NONE, true, false);
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

Delta *history_get_last_delta(History *history)
{
    if (history->command_count < 1) return NULL;
    Command *command = &history->commands[history->command_count - 1];
    if (command->committed || command->delta_count < 1) return NULL;
    Delta *delta = &command->deltas[command->delta_count - 1];
    return delta;
}

Command *history_get_last_uncommitted_command(History *history)
{
    if (history->command_count > 0)
    {
        Command *c = &history->commands[history->command_count - 1];
        if (!c->committed)
        {
            return c;
        }
    }
    return NULL;
}

Command *history_get_command_to_undo(History *history)
{
    if (history_get_last_uncommitted_command(history))
    {
        history_commit_command(history);
    }

    if (history->command_count > 0 && history->history_pos > 0)
    {
        return &history->commands[history->history_pos - 1];
    }
    return NULL;
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
