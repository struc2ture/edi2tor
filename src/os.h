#pragma once

#include <stdlib.h>

#include "types.h"

typedef enum File_Kind
{
    FILE_KIND_NONE,
    FILE_KIND_TEXT,
    FILE_KIND_IMAGE,
    FILE_KIND_DYLIB
} File_Kind;

struct Editor_State;

void os_read_clipboard(char *buf, size_t buf_size);
void os_write_clipboard(const char *text);

char *os_get_working_dir();
bool os_change_working_dir(const char *dir, struct Editor_State *state);

bool os_file_exists(const char *path);
bool os_file_is_image(const char *path);
File_Kind os_file_detect_kind(const char *path);
