#include "os.h"

#include <stdlib.h>
#include <stdio.h>

#include "editor.h"
#include "util.h"

void os_read_clipboard(char *buf, size_t buf_size)
{
    FILE *pipe = popen("pbpaste", "r");
    if (!pipe) return;
    size_t len = 0;
    while (fgets(buf + len, buf_size - len, pipe))
    {
        len = strlen(buf);
        if (len >= buf_size - 1) break;
    }
    pclose(pipe);
}

void os_write_clipboard(const char *text)
{
    FILE *pipe = popen("pbcopy", "w");
    if (!pipe) return;
    fputs(text, pipe);
    pclose(pipe);
}

char *os_get_working_dir()
{
    char *dir = xmalloc(1024);
    getcwd(dir, 1024);
    return dir;
}

bool os_change_working_dir(const char *dir, Editor_State *state)
{
    // TODO: Don't modify Editor_State here
    if (chdir(dir) == 0)
    {
        if (state->working_dir) free(state->working_dir);
        state->working_dir = os_get_working_dir();
        return true;
    }
    log_warning("Failed to change working dir to %s", dir);
    return false;
}

bool os_file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f)
    {
        fclose(f);
        return true;
    }
    return false;
}

bool os_file_is_image(const char *path)
{
    int x, y, comp;
    return stbi_info(path, &x, &y, &comp) != 0;
}

File_Kind os_file_detect_kind(const char *path)
{
    if (!os_file_exists(path)) return FILE_KIND_NONE;
    const char *ext = strrchr(path, '.');
    if (ext && strcmp(ext, ".dylib") == 0) return FILE_KIND_DYLIB;
    if (os_file_is_image(path)) return FILE_KIND_IMAGE;
    return FILE_KIND_TEXT;
}
