#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline static void bassert(bool condition)
{
    if (!condition) __builtin_debugtrap();
}

static void trace_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[EDITOR] ");
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
