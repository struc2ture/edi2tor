#pragma once

#include "common.h"

typedef enum Platform_Event_Kind {
    PLATFORM_EVENT_CHAR,
    PLATFORM_EVENT_KEY,
    PLATFORM_EVENT_MOUSE_BUTTON,
    PLATFORM_EVENT_MOUSE_MOTION,
    PLATFORM_EVENT_MOUSE_SCROLL,
    PLATFORM_EVENT_WINDOW_RESIZE,
    PLATFORM_EVENT_INPUT_CAPTURED,
} Platform_Event_Kind;

typedef struct Platform_Event {
    union
    {
        struct
        {
            unsigned int codepoint;
        } character;

        struct
        {
            int key;
            int scancode;
            int action;
            int mods;
        } key;

        struct
        {
            int button;
            int action;
            int mods;
            v2 pos;
        } mouse_button;

        struct
        {
            v2 pos;
            v2 delta;
        } mouse_motion;

        struct
        {
            v2 scroll;
            v2 pos;
        } mouse_scroll;

        struct
        {
            int px_w;
            int px_h;
            int logical_w;
            int logical_h;
        } window_resize;

        struct
        {
            bool captured;
        } input_captured;
    };
    Platform_Event_Kind kind;
} Platform_Event;

typedef struct Platform_Timing {
    float prev_frame_time;
    float prev_prev_frame_time;
    float prev_delta_time;
    float last_fps_measurement_time;
    int frame_running_count;
    int frame_total_count;
    float fps_avg;
    float fps_instant;
} Platform_Timing;
