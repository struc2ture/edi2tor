#pragma once

#include "types.h"

typedef struct Color
{
    u8 r, g, b, a;
}
Color;

static inline u32 color_to_u32(Color c)
{
    return ((u32)c.r << 24) | ((u32)c.g << 16) | ((u32)c.b << 8) | ((u32)c.a);
}

static inline Color color_from_u32(u32 rgba) {
    Color c;
    c.r = (rgba >> 24) & 0xFF;
    c.g = (rgba >> 16) & 0xFF;
    c.b = (rgba >> 8)  & 0xFF;
    c.a = rgba & 0xFF;
    return c;
}
