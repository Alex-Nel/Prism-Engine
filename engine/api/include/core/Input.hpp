#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include "Math.hpp"
#include "../PrismAPI.hpp"



namespace Prism
{
    // Code Name for specific keyboard keys
    enum class KeyCode
    {
        UNKNOWN = 0,

        A, B, C, D, E, F, G, H, I,
        J, K, L, M, N, O, P, Q, R,
        S, T, U, V, W, X, Y, Z,
        
        N0, N1, N2, N3, N4, N5, N6, N7, N8, N9,

        ESCAPE,
        F1, F2, F3, F4, F5, F6,
        F7, F8, F9, F10, F11, F12,
        DELETE,

        GRAVE, TILDE,
        EXCLAMATION, AT, HASH, DOLLAR, PERCENT, CARET, AMPERSAND, ASTERISK,
        LEFTPAREN, RIGHTPAREN, MINUS, EQUALS, UNDERSCORE, PLUS,
        BACKSPACE,

        TAB, LEFTBRACKET, RIGHTBRACKET, BACKSLASH, LEFTBRACE, RIGHTBRACE, PIPE,

        CAPSLOCK, SEMICOLON, APOSTROPHE, COLON, QUOTE, ENTER,

        COMMA, PERIOD, SLASH, LESS, GREATER, QUESTION,

        SPACE,

        LEFTSHIFT, RIGHTSHIFT,
        LEFTCTRL, RIGHTCTRL,
        LEFTALT, RIGHTALT,
        
        UPARROW, RIGHTARROW, DOWNARROW, LEFTARROW,
        
        MAX
    };


    // Code Name for specific mouse buttons
    enum class MouseButton
    {
        LEFT,
        RIGHT,
        MIDDLE,
        MAX
    };

    class Engine;

    // --- Contains functions that deal with user input ---
    class PRISM_API Input
    {
    private:
        friend class Engine;
        
        // Maps custom string to a physics key
        static std::unordered_map<std::string, KeyCode> s_ActionMap;

        // Maps a physical key to a list of lambda functions
        static std::unordered_map<KeyCode, std::vector<std::function<void()>>> s_KeyPressedCallbacks;

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
        // Mouse Button State
        // ==========================================

        static bool IsMouseButtonDown(MouseButton button);
        static bool IsMouseButtonPressed(MouseButton button);
        static bool IsMouseButtonReleased(MouseButton button);



        // ==========================================
        // Mouse Movement
        // ==========================================

        static Vector2 GetMousePosition();
        static Vector2 GetMouseDelta();
        static float GetMouseDeltaX();
        static float GetMouseDeltaY();
        static float GetMouseScrollDelta();



        // ==========================================
        // Action Mapping
        // ==========================================

        static void RegisterAction(const std::string& actionName, KeyCode key);
        static bool IsActionPressed(const std::string& actionName);
        static void BindKeyPressed(KeyCode key, std::function<void()> callback);
        static void DispatchCallbacks();

    private:
        static void Clear();
    };
}