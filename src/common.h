#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "types.h"

typedef enum Cardinal_Direction {
    DIR_NONE,
    DIR_LEFT,
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN
} Cardinal_Direction;

static inline v2 vec2_sub(v2 a, v2 b)
{
    return V2(a.x - b.x, a.y - b.y);
}

static inline m4 mat4_identity()
{
    m4 m;
    m.d[0] = 1; m.d[4] = 0; m.d[ 8] = 0; m.d[12] = 0;
    m.d[1] = 0; m.d[5] = 1; m.d[ 9] = 0; m.d[13] = 0;
    m.d[2] = 0; m.d[6] = 0; m.d[10] = 1; m.d[14] = 0;
    m.d[3] = 0; m.d[7] = 0; m.d[11] = 0; m.d[15] = 1;
    return m;
}

static inline m4 mat4_mul(m4 a, m4 b)
{
    m4 m;
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            m.d[col * 4 + row] = 0.0f;
            for (int k = 0; k < 4; k++)
            {
                m.d[col * 4 + row] += a.d[k * 4 + row] * b.d[col * 4 + k];
            }
        }
    }
    return m;
}

static inline m4 mat4_proj_ortho(float left, float right, float bottom, float top, float near, float far)
{
    m4 m;
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;

    m.d[0]  = 2.0f / rl;
    m.d[1]  = 0;
    m.d[2]  = 0;
    m.d[3]  = 0;

    m.d[4]  = 0;
    m.d[5]  = 2.0f / tb;
    m.d[6]  = 0;
    m.d[7]  = 0;

    m.d[8]  = 0;
    m.d[9]  = 0;
    m.d[10] = -2.0f / fn;
    m.d[11] = 0;

    m.d[12] = -(right + left) / rl;
    m.d[13] = -(top + bottom) / tb;
    m.d[14] = -(far + near) / fn;
    m.d[15] = 1.0f;

    return m;
}

static inline m4 mat4_translate(float x, float y, float z)
{
    m4 m = mat4_identity();
    m.d[12] = x;
    m.d[13] = y;
    m.d[14] = z;
    return m;
}

static inline m4 mat4_scale(float x, float y, float z)
{
    m4 m = mat4_identity();
    m.d[0] = x;
    m.d[5] = y;
    m.d[10] = z;
    return m;
}
