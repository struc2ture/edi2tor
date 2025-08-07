#include "history.h"

#include "text_buffer.h"
#include "util.h"

bool history_begin_command_0(History *history,
    Cursor_Pos cursor_pos,
    Text_Mark mark,
    const char *command_name,
    Running_Command_Kind running_command_kind,
    bool can_interrupt,
    bool reset_history)
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
    history->commands[history->command_count - 1].cursor_pos = cursor_pos;
    history->commands[history->command_count - 1].mark = mark;
    history->commands[history->command_count - 1].running_command_kind = running_command_kind;

    return true;
}

bool history_begin_command_running(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name, Running_Command_Kind running_kind)
{
    return history_begin_command_0(history, cursor_pos, mark, command_name, running_kind, true, true);
}

bool history_begin_command_non_interrupt(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name)
{
    return history_begin_command_0(history, cursor_pos, mark, command_name, RUNNING_COMMAND_NONE, false, true);
}

bool history_begin_command_non_reset(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name)
{
    return history_begin_command_0(history, cursor_pos, mark, command_name, RUNNING_COMMAND_NONE, true, false);
}

bool history_begin_command(History *history, Cursor_Pos cursor_pos, Text_Mark mark, const char *command_name)
{
    return history_begin_command_0(history, cursor_pos, mark, command_name, RUNNING_COMMAND_NONE, true, true);
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
    if (history->command_count > 0)
    {
        history->commands[history->command_count - 1].committed = true;
    }
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
    if (history->command_count > 0 && history->history_pos > 0)
    {
        return &history->commands[history->history_pos - 1];
    }
    return NULL;
}
