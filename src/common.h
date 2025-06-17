#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x, y;
} Vec_2;
#define VEC2_FMT "<%0.2f, %0.2f>"
#define VEC2_ARG(v) (v).x, (v).y

typedef struct {
    float x, y;
    float w, h;
} Rect;

typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} Rect_Bounds;

static inline Vec_2 vec2_sub(Vec_2 a, Vec_2 b)
{
    return (Vec_2){a.x - b.x, a.y - b.y};
}
