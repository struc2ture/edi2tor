#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

typedef struct v2 { union { struct { f32 x; f32 y; }; struct { f32 d[2]; }; }; } v2;
typedef struct v3 { union { struct { f32 x; f32 y; f32 z; }; struct { f32 r; f32 g; f32 b; }; struct { f32 d[3]; }; }; } v3;
typedef struct v4 { union { struct { f32 x; f32 y; f32 z; f32 w; }; struct { f32 r; f32 g; f32 b; f32 a; }; struct { f32 d[4]; }; }; } v4;
typedef struct v2i { union { struct { i32 x; i32 y; }; struct { i32 d[2]; }; }; } v2i;
typedef struct v3i { union { struct { i32 x; i32 y; i32 z; }; struct { i32 r; i32 g; i32 b; }; struct { i32 d[3]; }; }; } v3i;
typedef struct v4i { union { struct { i32 x; i32 y; i32 z; i32 w; }; struct { i32 r; i32 g; i32 b; i32 a; }; struct { i32 d[4]; }; }; } v4i;

/*
 * Column-major layout in memory
 * m[col][row]
 * m[col * 4 + row]
 * M00 M10 M20 M30
 * M01 M11 M21 M31
 * M02 M12 M22 M32
 * M03 M13 M23 M33
 * In memory:
 * M00 M01 M02 M03 M10 M11 ...
 */
typedef struct m4 { f32 d[16]; } m4;

static inline v2 V2(f32 x, f32 y) { return (v2){{{x, y}}}; }
static inline v3 V3(f32 x, f32 y, f32 z) { return (v3){{{x, y, z}}}; }
static inline v4 V4(f32 x, f32 y, f32 z, f32 w) { return (v4){{{x, y, z, w}}}; }
static inline v2i V2I(i32 x, i32 y) { return (v2i){{{x, y}}}; }
static inline v3i V3I(i32 x, i32 y, i32 z) { return (v3i){{{x, y, z}}}; }
static inline v4i V4I(i32 x, i32 y, i32 z, i32 w) { return (v4i){{{x, y, z, w}}}; }

#define V2_ZERO ((v2){{{}}})

#define V3_ZERO ((v3){{{}}})
#define V3_RIGHT ((v3){{{1.0f, 0.0f, 0.0f}}})
#define V3_UP ((v3){{{0.0f, 1.0f, 0.0f}}})
#define V3_FORWARD ((v3){{{1.0f, 0.0f, 1.0f}}})

#define V4_ZERO ((v4){{{}}})

#define globvar static
