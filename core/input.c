#include "input.h"
#include <string.h>

#define MAX_KEYS 256
#define MAX_MOUSE_BUTTONS 8

// --- State Arrays ---
static bool current_keys[KEYCODE_MAX];
static bool previous_keys[KEYCODE_MAX];

static bool current_mouse_buttons[MOUSE_BUTTON_MAX];
static bool previous_mouse_buttons[MOUSE_BUTTON_MAX];

// --- Mouse Data ---
static float mouse_x, mouse_y;
static float mouse_delta_x, mouse_delta_y;
static float mouse_scroll_delta;



void Input_Init(void) {
    memset(current_keys, 0, sizeof(current_keys));
    memset(previous_keys, 0, sizeof(previous_keys));
    memset(current_mouse_buttons, 0, sizeof(current_mouse_buttons));
    memset(previous_mouse_buttons, 0, sizeof(previous_mouse_buttons));
    
    mouse_x = 0; mouse_y = 0;
    mouse_delta_x = 0; mouse_delta_y = 0;
    mouse_scroll_delta = 0.0f;
}



void Input_ProcessEvent(const Event* event) {
    switch (event->type) {
        case EVENT_KEY_PRESSED:
            current_keys[event->key.key] = true;
            break;
            
        case EVENT_KEY_RELEASED:
            current_keys[event->key.key] = false;
            break;
            
        case EVENT_MOUSE_BUTTON_PRESSED:
            current_mouse_buttons[event->mouse_button.button] = true;
            break;
            
        case EVENT_MOUSE_BUTTON_RELEASED:
            current_mouse_buttons[event->mouse_button.button] = false;
            break;
            
        case EVENT_MOUSE_MOVED:
            // mouse_delta_x += (event->mouse_state.x - mouse_x);
            // mouse_delta_y += (event->mouse_state.y - mouse_y);
            mouse_delta_x += event->mouse_state.dx;
            mouse_delta_y += event->mouse_state.dy;

            mouse_x = event->mouse_state.x;
            mouse_y = event->mouse_state.y;
            
            break;
            
        case EVENT_MOUSEWHEEL_SCROLLED:
            mouse_scroll_delta += event->mouse_scroll.delta_y;
            break;
            
        default:
            break; // Not an input event, ignore
    }
}

void Input_Update(void) {
    // 1. Copy current state to previous state for the NEXT frame
    memcpy(previous_keys, current_keys, sizeof(current_keys));
    memcpy(previous_mouse_buttons, current_mouse_buttons, sizeof(current_mouse_buttons));
    
    // 2. Reset deltas (they only persist for a single frame)
    mouse_delta_x = 0;
    mouse_delta_y = 0;
    mouse_scroll_delta = 0.0f;
}

// --- Query Functions ---

bool Input_IsKeyDown(KeyCode key) {
    return current_keys[key];
}

bool Input_IsKeyPressed(KeyCode key) {
    return current_keys[key] && !previous_keys[key];
}

bool Input_IsKeyReleased(KeyCode key) {
    return !current_keys[key] && previous_keys[key];
}





// --- Mouse State ---
bool Input_IsMouseButtonDown(MouseButton button)
{
    return current_mouse_buttons[button];
}


bool Input_IsMouseButtonPressed(MouseButton button)
{
    return current_mouse_buttons[button] && !previous_mouse_buttons[button];
}


bool Input_IsMouseButtonReleased(MouseButton button)
{
    return !current_mouse_buttons[button] && previous_mouse_buttons[button];
}

// Mouse Position
void Input_GetMousePosition(int32_t* x, int32_t* y)
{
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
}


void Input_GetMouseDelta(int32_t* dx, int32_t* dy)
{
    if (dx) *dx = mouse_delta_x;
    if (dy) *dy = mouse_delta_y;
}


float Input_GetMouseDeltaX()
{
    return mouse_delta_x;
}


float Input_GetMouseDeltaY()
{
    return mouse_delta_y;
}


float Input_GetMouseScrollDelta()
{
    return mouse_scroll_delta;
}