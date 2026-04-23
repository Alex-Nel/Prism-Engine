#pragma once

#include "Math.hpp"

extern "C"
{
    #include "core/input.h" 
    #include "core/event.h" // Needed for KeyCode and MouseButton enums
}

namespace Prism
{
    class Input
    {
    public:
        // Prevent accidental instantiation 
        Input() = delete;


        // ==========================================
        // Keyboard State
        // ==========================================

        static bool IsKeyDown(KeyCode key) {
            return ::Input_IsKeyDown(key);
        }

        static bool IsKeyPressed(KeyCode key) {
            return ::Input_IsKeyPressed(key);
        }

        static bool IsKeyReleased(KeyCode key) {
            return ::Input_IsKeyReleased(key);
        }


        // ==========================================
        // MOUSE BUTTON STATE
        // ==========================================

        static bool IsMouseButtonDown(MouseButton button) {
            return ::Input_IsMouseButtonDown(button);
        }

        static bool IsMouseButtonPressed(MouseButton button) {
            return ::Input_IsMouseButtonPressed(button);
        }

        static bool IsMouseButtonReleased(MouseButton button) {
            return ::Input_IsMouseButtonReleased(button);
        }


        // ==========================================
        // MOUSE MOVEMENT
        // ==========================================

        // Return a Prism::Vector2 instead of requiring pointers
        static Vector2 GetMousePosition() {
            int32_t x = 0, y = 0;
            ::Input_GetMousePosition(&x, &y);
            return Vector2(static_cast<float>(x), static_cast<float>(y));
        }

        static Vector2 GetMouseDelta() {
            return Vector2(::Input_GetMouseDeltaX(), ::Input_GetMouseDeltaY());
        }

        static float GetMouseDeltaX() {
            return ::Input_GetMouseDeltaX();
        }

        static float GetMouseDeltaY() {
            return ::Input_GetMouseDeltaY();
        }

        static float GetMouseScrollDelta() {
            return ::Input_GetMouseScrollDelta();
        }
    };
}