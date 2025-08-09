#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x, y;
} Vec_2;
#define VEC2_FMT "<%0.2f, %0.2f>"
#define VEC2_ARG(v) (v).x, (v).y

typedef struct {
    float x, y, z;
} Vec_3;

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
} Dir;

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

static inline Vec_3 mat4_mul_vec3(Mat_4 m, Vec_3 v)
{
    float x = v.x, y = v.y, z = v.z, w = 1.0f;
    Vec_3 result;
    result.x = m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12] * w;
    result.y = m.m[1] * x + m.m[5] * y + m.m[9] * z + m.m[13] * w;
    result.z = m.m[2] * x + m.m[6] * y + m.m[10] * z + m.m[14] * w;
    return result;
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

static inline Mat_4 mat4_proj_perspective(float fov, float aspect, float znear, float zfar)
{
    float tan_half = tanf(fov / 2.0f);

    Mat_4 m = {0};
    m.m[0] = 1.0f / (aspect * tan_half);
    m.m[5] = 1.0f / tan_half;
    m.m[10] = -(zfar + znear) / (zfar - znear);
    m.m[11] = -1.0f;
    m.m[14] = -(2.0f * zfar * znear) / (zfar - znear);

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

static inline Mat_4 mat4_rotate_axis(float x, float y, float z, float theta)
{
    float c = cosf(theta);
    float s = sinf(theta);
    float t = 1.0f - c;

    Mat_4 m;
    m.m[0]  = t*x*x + c;
    m.m[1]  = t*x*y + s*z;
    m.m[2]  = t*x*z - s*y;
    m.m[3]  = 0;

    m.m[4]  = t*x*y - s*z;
    m.m[5]  = t*y*y + c;
    m.m[6]  = t*y*z + s*x;
    m.m[7]  = 0;

    m.m[8]  = t*x*z + s*y;
    m.m[9]  = t*y*z - s*x;
    m.m[10] = t*z*z + c;
    m.m[11] = 0;

    m.m[12] = 0;
    m.m[13] = 0;
    m.m[14] = 0;
    m.m[15] = 1;

    return m;
}

static inline Vec_3 vec3_normalize(Vec_3 v)
{
    float mag = sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
    if  (mag == 0.0f) return (Vec_3){0};
    float i_mag = 1.0f / mag;
    Vec_3 result = {
        .x = v.x * i_mag,
        .y = v.y * i_mag,
        .z = v.z * i_mag
    };
    return result;
}

static inline Vec_3 vec3_cross(Vec_3 a, Vec_3 b)
{
    Vec_3 result = {
        .x = a.y*b.z - a.z*b.y,
        .y = a.z*b.x - a.x*b.z,
        .z = a.x*b.y - a.y*b.x
    };
    return result;
}

static inline Vec_3 vec3_sub(Vec_3 a, Vec_3 b)
{
    Vec_3 result = {
        .x = a.x - b.x,
        .y = a.y - b.y,
        .z = a.z - b.z,
    };
    return result;
}

static inline float vec3_dot(Vec_3 a, Vec_3 b)
{
    float result = a.x*b.x + a.y*b.y + a.z*b.z;
    return result;
}

static inline Vec_3 vec3_add(Vec_3 a, Vec_3 b)
{
    Vec_3 result = {
        .x = a.x + b.x,
        .y = a.y + b.y,
        .z = a.z + b.z
    };
    return result;
}

static inline Vec_3 vec3_scale(Vec_3 v, float s)
{
    Vec_3 result = {
        .x = v.x * s,
        .y = v.y * s,
        .z = v.z * s
    };
    return result;
}

static inline Mat_4 mat4_look_at(Vec_3 eye, Vec_3 target, Vec_3 up)
{
    Vec_3 f = vec3_normalize(vec3_sub(target, eye));
    Vec_3 r = vec3_normalize(vec3_cross(f, up));
    Vec_3 u = vec3_cross(r, f);

    Mat_4 m;
    m.m[0] = r.x;
    m.m[1] = u.x;
    m.m[2] = -f.x;
    m.m[3] = 0;

    m.m[4] = r.y;
    m.m[5] = u.y;
    m.m[6] = -f.y;
    m.m[7] = 0;

    m.m[8] = r.z;
    m.m[9] = u.z;
    m.m[10] = -f.z;
    m.m[11] = 0;

    m.m[12] = -vec3_dot(r, eye);
    m.m[13] = -vec3_dot(u, eye);
    m.m[14] = vec3_dot(f, eye);
    m.m[15] = 1;

    return m;
}
