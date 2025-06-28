#pragma once

#include <ctype.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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
#define trace_log(FMT, ...) _trace_log("%s: " FMT, __func__, ##__VA_ARGS__)

static void log_warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[EDITOR][WARNING] ");
    vprintf(fmt, args);
    va_end(args);
    putchar('\n');
}

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

static void * xdlsym(void *handle, const char *name)
{
    void *sym = dlsym(handle, name);
    if (!sym) {
        fprintf(stderr, "[PLATFORM] dlsym error: %s\n", dlerror());
        exit(1);
    }
    return sym;
}
