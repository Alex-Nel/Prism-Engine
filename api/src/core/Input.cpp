#include "../../include/core/Input.hpp"


extern "C"
{
    #include "../../../core/input.h" 
    #include "../../../core/event.h"
}


namespace Prism
{
    // ----- Definitions of the static unordered maps -----

    std::unordered_map<std::string, KeyCode> Input::s_ActionMap;
    std::unordered_map<KeyCode, std::vector<std::function<void()>>> Input::s_KeyPressedCallbacks;


    
    // ==========================================
    // Keyboard State
    // ==========================================

    bool Input::IsKeyDown(KeyCode key) {
        return ::Input_IsKeyDown(static_cast<::KeyCode>(key));
    }
    bool Input::IsKeyPressed(KeyCode key) {
        return ::Input_IsKeyPressed(static_cast<::KeyCode>(key));
    }
    bool Input::IsKeyReleased(KeyCode key) {
        return ::Input_IsKeyReleased(static_cast<::KeyCode>(key));
    }



    // ==========================================
    // Mouse Button State
    // ==========================================

    bool Input::IsMouseButtonDown(MouseButton button) {
        return ::Input_IsMouseButtonDown(static_cast<::MouseButton>(button));
    }
    bool Input::IsMouseButtonPressed(MouseButton button) {
        return ::Input_IsMouseButtonPressed(static_cast<::MouseButton>(button));
    }
    bool Input::IsMouseButtonReleased(MouseButton button) {
        return ::Input_IsMouseButtonReleased(static_cast<::MouseButton>(button));
    }



    // ==========================================
    // Mouse Movement
    // ==========================================

    Vector2 Input::GetMousePosition() {
        float x = 0, y = 0;
        ::Input_GetMousePosition(&x, &y);
        return Vector2(x, y);
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



    // ==========================================
    // Action Mapping
    // ==========================================

    void Input::RegisterAction(const std::string& actionName, KeyCode key) {
        s_ActionMap[actionName] = key;
    }

    bool Input::IsActionPressed(const std::string& actionName) {
        auto it = s_ActionMap.find(actionName);
        if (it != s_ActionMap.end()) {
            ::KeyCode cCode = ::KeyCode(it->second);
            return ::Input_IsKeyPressed(cCode); 
        }
        return false;
    }

    void Input::BindKeyPressed(KeyCode key, std::function<void()> callback) {
        s_KeyPressedCallbacks[key].push_back(callback);
    }

    // Called once per frame by the C++ Engine Loop
    void Input::DispatchCallbacks() {
        for (const auto& pair : s_KeyPressedCallbacks) {
            if (::Input_IsKeyPressed(::KeyCode(pair.first))) {
                for (const auto& callback : pair.second) {
                    callback();
                }
            }
        }
    }

    // Called at shutdown to prevent Segmentation Faults!
    void Input::Clear() {
        s_ActionMap.clear();
        s_KeyPressedCallbacks.clear();
    }
}