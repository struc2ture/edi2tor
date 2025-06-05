#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"

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

static void log_warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printf("[EDITOR][WARNING]");
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

inline static Rect_Bounds rect_get_bounds(Rect r)
{
    Rect_Bounds b;
    b.min_x = r.x;
    b.min_y = r.y;
    b.max_x = r.x + r.w;
    b.max_y = r.y + r.h;
    return b;
}

inline static bool rect_intersect(Rect a, Rect b)
{
    float a_min_x = a.x;
    float a_min_y = a.y;
    float a_max_x = a.x + a.w;
    float a_max_y = a.y + a.h;
    float b_min_x = b.x;
    float b_min_y = b.y;
    float b_max_x = b.x + b.w;
    float b_max_y = b.y + b.h;
    bool intersect =
        a_min_x < b_max_x && a_max_x > b_min_x &&
        a_min_y < b_max_y && a_max_y > b_min_y;
    return intersect;
}

inline static bool rect_p_intersect(Vec_2 p, Rect rect)
{
    float min_x = rect.x;
    float min_y = rect.y;
    float max_x = rect.x + rect.w;
    float max_y = rect.y + rect.h;
    bool intersect =
        p.x > min_x && p.x < max_x &&
        p.y > min_y && p.y < max_y;
    return intersect;
}
