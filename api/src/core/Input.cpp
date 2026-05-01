#include "../../include/core/Input.hpp"


extern "C"
{
    #include "../../../core/input.h" 
    #include "../../../core/event.h"
}


namespace Prism
{
    bool Input::IsKeyDown(KeyCode key) {
        return ::Input_IsKeyDown(static_cast<::KeyCode>(key));
    }
    bool Input::IsKeyPressed(KeyCode key) {
        return ::Input_IsKeyPressed(static_cast<::KeyCode>(key));
    }
    bool Input::IsKeyReleased(KeyCode key) {
        return ::Input_IsKeyReleased(static_cast<::KeyCode>(key));
    }


    bool Input::IsMouseButtonDown(MouseButton button) {
        return ::Input_IsMouseButtonDown(static_cast<::MouseButton>(button));
    }
    bool Input::IsMouseButtonPressed(MouseButton button) {
        return ::Input_IsMouseButtonPressed(static_cast<::MouseButton>(button));
    }
    bool Input::IsMouseButtonReleased(MouseButton button) {
        return ::Input_IsMouseButtonReleased(static_cast<::MouseButton>(button));
    }


    Vector2 Input::GetMousePosition() {
        int32_t x = 0, y = 0;
        ::Input_GetMousePosition(&x, &y);
        return Vector2(static_cast<float>(x), static_cast<float>(y));
    }


    Vector2 Input::GetMouseDelta() {
        return Vector2(::Input_GetMouseDeltaX(), ::Input_GetMouseDeltaY());
    }
    float Input::GetMouseDeltaX() {
        return ::Input_GetMouseDeltaX();
    }
    float Input::GetMouseDeltaY() {
        return ::Input_GetMouseDeltaY();
    }
    float Input::GetMouseScrollDelta() {
        return ::Input_GetMouseScrollDelta();
    }
}