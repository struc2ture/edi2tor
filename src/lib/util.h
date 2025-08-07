#pragma once

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

inline static void bassert(bool condition)
{
    if (!condition) __builtin_debugtrap();
}

static void _trace_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[EDITOR] ");
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
}
#define trace_log(FMT, ...) _trace_log("{%s:%d(%s)} " FMT, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

static void _log_warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[EDITOR][WARNING] ");
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
}

#define log_warning(FMT, ...) _log_warning("{%s:%d(%s)} " FMT, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

static void fatal(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    bassert(false);
    exit(1);
}

static void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr) fatal("malloc failed for %zu", size);
    return ptr;
}

static void *xcalloc(size_t size)
{
    void *ptr = calloc(1, size);
    if (!ptr) fatal("calloc failed for %zu", size);
    return ptr;
}

static void *xrealloc(void *ptr, size_t size)
{
    void *new_ptr = realloc(ptr, size);
    if (!new_ptr) fatal("realloc failed");
    return new_ptr;
}

static char *xstrdup(const char *str)
{
    char *new_str = strdup(str);
    if (!new_str) fatal("strdup failed");
    return new_str;
}

static char *xstrndup(const char *str, size_t size)
{
    char *new_str = strndup(str, size);
    if (!new_str) fatal("strndup failed");
    return new_str;
}

static int xstrtoint(const char *str)
{
    int x;
    char *end;
    x = (int)strtol(str, &end, 10);
    if (*end != '\0') {
        log_warning("xstrtoint: Could not convert '%s' to int. Returning %d", str, x);
    }
    return x;
}

static void flip_bitmap(void *bitmap_bytes, int pitch, int height)
{
    unsigned char *b = bitmap_bytes;
    unsigned char *temp_row = xmalloc(pitch);
    for (int row_a = 0, row_b = height - 1; row_a < height / 2; row_a++, row_b--)
    {
        memcpy(temp_row, &b[row_a * pitch], pitch);
        memcpy(&b[row_a * pitch], &b[row_b * pitch], pitch);
        memcpy(&b[row_b * pitch], temp_row, pitch);
    }
    free(temp_row);
}

static int str_get_line_segment_count(const char *str)
{
    int newline_count = 0;
    while (*str != '\0')
    {
        if (*str == '\n') newline_count++;
        str++;
    }
    return newline_count + 1;
}

static int str_find_next_new_line(const char *str, int start_at)
{
    const char *s = str + start_at;
    int index = start_at;
    while (*s != '\0')
    {
        if (*s == '\n')
            return index;
        s++;
        index++;
    }
    return index;
}

static bool is_white_line(const char *str)
{
    while (*str)
    {
        if (!isspace(*str))
        {
            return false;
        }
        str++;
    }
    return true;
}

static time_t get_file_timestamp(const char *path) {
    struct stat attr;
    if (stat(path, &attr) == 0) {
        return attr.st_mtime;
    }
    fprintf(stderr, "[PLATFORM] Failed to get timestamp for file at %s\n", path);
    exit(1);
}

static void *xdlopen(const char *dl_path)
{
    void *handle = dlopen(dl_path, RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "[PLATFORM] dlopen error: %s\n", dlerror());
        exit(1);
    }
    return handle;
}

static void *xdlsym(void *handle, const char *name)
{
    void *sym = dlsym(handle, name);
    if (!sym) {
        fprintf(stderr, "[PLATFORM] dlsym error: %s\n", dlerror());
        exit(1);
    }
    return sym;
}

static bool file_delete(const char *path)
{
    if (remove(path) == 0)
    {
        trace_log("Deleted file at %s", path);
        return true;
    }
    else
    {
        log_warning("Failed to deleted file at %s ", path);
        return false;
    }
}

static void file_write(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    if (!f)
    {
        log_warning("Failed to open file at %s", path);
        return;
    }

    fputs(content, f);
    fclose(f);
}

static void clear_dir(const char *path)
{
    DIR *d = opendir(path);
    if (!d)
    {
        log_warning("Failed to open dir at %s", path);
        return;
    }

    struct dirent *entry;
    char filepath[1024];

    while ((entry = readdir(d)))
    {
        if (entry->d_type != DT_REG) continue; // only delete regular files
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        unlink(filepath);
    }

    closedir(d);
}

static char *read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        log_warning("Failed to open file for reading at %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = xmalloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    if (out_size) *out_size = size;
    fclose(f);
    return buf;
}
