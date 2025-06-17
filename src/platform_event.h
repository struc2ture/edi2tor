#pragma once

typedef enum {
    PLATFORM_EVENT_CHAR,
    PLATFORM_EVENT_KEY,
    PLATFORM_EVENT_MOUSE_BUTTON,
    PLATFORM_EVENT_SCROLL,
    PLATFORM_EVENT_WINDOW_RESIZE
} Platform_Event_Kind;

typedef struct {
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
        } mouse_button;

        struct
        {
            double x_offset;
            double y_offset;
        } scroll;

        struct
        {
            int px_w;
            int px_h;
            int logical_w;
            int logical_h;
        } window_resize;
    };
    Platform_Event_Kind kind;
} Platform_Event;
