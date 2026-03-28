#ifndef CORE_EVENT_H
#define CORE_EVENT_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_EVENTS 256


typedef enum KeyCode
{
    KEYCODE_UNKNOWN = 0,
    KEYCODE_A, KEYCODE_B, KEYCODE_C, KEYCODE_D, KEYCODE_E, KEYCODE_F, KEYCODE_G, KEYCODE_H,
    KEYCODE_I, KEYCODE_J, KEYCODE_K, KEYCODE_L, KEYCODE_M, KEYCODE_N, KEYCODE_O, KEYCODE_P,
    KEYCODE_Q, KEYCODE_R, KEYCODE_S, KEYCODE_T, KEYCODE_U, KEYCODE_V, KEYCODE_W, KEYCODE_X,
    KEYCODE_Y, KEYCODE_Z,
    KEYCODE_0, KEYCODE_1, KEYCODE_2, KEYCODE_3, KEYCODE_4, KEYCODE_5,
    KEYCODE_6, KEYCODE_7, KEYCODE_8, KEYCODE_9,
    KEYCODE_SPACE, KEYCODE_ESCAPE, KEYCODE_ENTER, KEYCODE_RIGHTSHIFT, KEYCODE_LEFTSHIFT,
    KEYCODE_LEFTCTRL, KEYCODE_RIGHTCTRL,
    KEYCODE_UPARROW, KEYCODE_RIGHTARROW, KEYCODE_DOWNARROW, KEYCODE_LEFTARROW,
    KEYCODE_MAX
} KeyCode;



typedef enum MouseButton
{
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_MAX
} MouseButton;


// Structure for event types
typedef enum EventType
{
    EVENT_NONE = 0,

    // Window
    EVENT_WINDOW_CLOSE,
    EVENT_WINDOW_RESIZE,
    EVENT_WINDOW_FOCUS_GAINED,
    EVENT_WINDOW_FOCUS_LOST,
    
    // Input
    EVENT_KEY_PRESSED,
    EVENT_KEY_RELEASED,

    EVENT_MOUSE_MOVED,
    EVENT_MOUSE_BUTTON_PRESSED,
    EVENT_MOUSE_BUTTON_RELEASED,
    EVENT_MOUSEWHEEL_SCROLLED

} EventType;



// Structure for events
typedef struct Event
{
    EventType type;
    union
    {
        // Window
        struct { uint32_t width, height; } window_resize;
        
        // Keyboard
        struct { KeyCode key; } key;
        
        // Mouse
        struct { float x, y, dx, dy; } mouse_state;
        struct { MouseButton button; } mouse_button;
        struct { float delta_x, delta_y; } mouse_scroll;
    };

} Event;


#endif