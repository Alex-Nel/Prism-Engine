#pragma once

#include "Math.hpp"



namespace Prism
{
    // Code Name for specific keyboard keys
    enum KeyCode
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
    };


    // Code Name for specific mouse buttons
    enum MouseButton
    {
        MOUSE_BUTTON_LEFT,
        MOUSE_BUTTON_RIGHT,
        MOUSE_BUTTON_MIDDLE,
        MOUSE_BUTTON_MAX
    };


    // --- Contains functions that deal with user input ---
    class Input
    {
    public:
        // Prevent accidental instantiation 
        Input() = delete;


        // ==========================================
        // Keyboard State
        // ==========================================

        static bool IsKeyDown(KeyCode key);
        static bool IsKeyPressed(KeyCode key);
        static bool IsKeyReleased(KeyCode key);



        // ==========================================
        // MOUSE BUTTON STATE
        // ==========================================

        static bool IsMouseButtonDown(MouseButton button);
        static bool IsMouseButtonPressed(MouseButton button);
        static bool IsMouseButtonReleased(MouseButton button);



        // ==========================================
        // MOUSE MOVEMENT
        // ==========================================

        // Return a Prism::Vector2 instead of requiring pointers
        static Vector2 GetMousePosition();
        static Vector2 GetMouseDelta();
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();
        static float GetMouseScrollDelta();
    };
}