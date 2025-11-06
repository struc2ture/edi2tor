#pragma once

#include "types.h"

typedef struct Rect
{
    f32 x, y;
    f32 w, h;
} Rect;

typedef struct Rect_Bounds
{
    f32 min_x, min_y;
    f32 max_x, max_y;
} Rect_Bounds;

static inline Rect_Bounds rect_get_bounds(Rect r)
{
    Rect_Bounds b;
    b.min_x = r.x;
    b.min_y = r.y;
    b.max_x = r.x + r.w;
    b.max_y = r.y + r.h;
    return b;
}

static inline bool rect_intersect(Rect a, Rect b)
{
    f32 a_min_x = a.x;
    f32 a_min_y = a.y;
    f32 a_max_x = a.x + a.w;
    f32 a_max_y = a.y + a.h;
    f32 b_min_x = b.x;
    f32 b_min_y = b.y;
    f32 b_max_x = b.x + b.w;
    f32 b_max_y = b.y + b.h;
    bool intersect =
        a_min_x < b_max_x && a_max_x > b_min_x &&
        a_min_y < b_max_y && a_max_y > b_min_y;
    return intersect;
}

static inline bool rect_contains_p(v2 p, Rect rect)
{
    f32 min_x = rect.x;
    f32 min_y = rect.y;
    f32 max_x = rect.x + rect.w;
    f32 max_y = rect.y + rect.h;
    bool intersect =
        p.x > min_x && p.x < max_x &&
        p.y > min_y && p.y < max_y;
    return intersect;
}
