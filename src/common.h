#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x, y;
} Vec_2;
#define VEC2_FMT "<%0.2f, %0.2f>"
#define VEC2_ARG(v) (v).x, (v).y

/*
 * OpenGL expects column-major.
 * m[column][row]
 * m[column * 4 + row]
 * M00 M01 M02 M03
 * M10 M11 M12 M13
 * M20 M21 M22 M23
 * M30 M31 M32 M33
 *
 * Laid out in memory like this:
 * M00 M10 M20 M30 M01 M11 ...
 */
typedef struct {
    float m[16];
} Mat_4;

typedef struct {
    float x, y;
    float w, h;
} Rect;

typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} Rect_Bounds;

typedef enum {
    DIR_NONE,
    DIR_LEFT,
    DIR_UP,
    DIR_RIGHT,
    DIR_DOWN
} Cardinal_Direction;

static inline Vec_2 vec2_sub(Vec_2 a, Vec_2 b)
{
    return (Vec_2){a.x - b.x, a.y - b.y};
}

static inline Mat_4 mat4_identity()
{
    Mat_4 m;
    m.m[0] = 1; m.m[4] = 0; m.m[ 8] = 0; m.m[12] = 0;
    m.m[1] = 0; m.m[5] = 1; m.m[ 9] = 0; m.m[13] = 0;
    m.m[2] = 0; m.m[6] = 0; m.m[10] = 1; m.m[14] = 0;
    m.m[3] = 0; m.m[7] = 0; m.m[11] = 0; m.m[15] = 1;
    return m;
}

static inline Mat_4 mat4_mul(Mat_4 a, Mat_4 b)
{
    Mat_4 m;
    for (int col = 0; col < 4; col++)
    {
        for (int row = 0; row < 4; row++)
        {
            m.m[col * 4 + row] = 0.0f;
            for (int k = 0; k < 4; k++)
            {
                m.m[col * 4 + row] += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
        }
    }
    return m;
}

static inline Mat_4 mat4_proj_ortho(float left, float right, float bottom, float top, float near, float far)
{
    Mat_4 m;
    float rl = right - left;
    float tb = top - bottom;
    float fn = far - near;

    m.m[0]  = 2.0f / rl;
    m.m[1]  = 0;
    m.m[2]  = 0;
    m.m[3]  = 0;

    m.m[4]  = 0;
    m.m[5]  = 2.0f / tb;
    m.m[6]  = 0;
    m.m[7]  = 0;

    m.m[8]  = 0;
    m.m[9]  = 0;
    m.m[10] = -2.0f / fn;
    m.m[11] = 0;

    m.m[12] = -(right + left) / rl;
    m.m[13] = -(top + bottom) / tb;
    m.m[14] = -(far + near) / fn;
    m.m[15] = 1.0f;

    return m;
}

static inline Mat_4 mat4_translate(float x, float y, float z)
{
    Mat_4 m = mat4_identity();
    m.m[12] = x;
    m.m[13] = y;
    m.m[14] = z;
    return m;
}

static inline Mat_4 mat4_scale(float x, float y, float z)
{
    Mat_4 m = mat4_identity();
    m.m[0] = x;
    m.m[5] = y;
    m.m[10] = z;
    return m;
}
