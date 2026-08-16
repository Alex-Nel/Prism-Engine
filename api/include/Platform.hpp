#pragma once

#include "PrismAPI.hpp"
#include <string>



namespace Prism
{

    // --- Class for platform functions ---
    class PRISM_API Platform
    {
    public:
        // Prevent accidental instantiation
        Platform() = delete;


        // --- Time ---

        // Gets the high-resolution time since the engine started
        static double GetTime();


        // --- Window & Mouse Management ---

        // Locks the mouse to the window and hides the cursor
        static void SetRelativeMouseMode(bool enabled);

        // Forces the mouse cursor to the center of the screen
        static void WarpMouseToMiddle();

        // Copies UTF-8 text to the system clipboard
        static bool SetClipboardText(const std::string& text);

        // Gets UTF-8 text from the system clipboard
        static std::string GetClipboardText();

        // Gets the width of the current window in pixels
        static int GetWindowWidth();

        // Gets the height of the current window in pixels
        static int GetWindowHeight();

        // Returns whether the window is currently minimized
        static bool IsWindowMinimized();

    private:
        // Gets the active window pointer from the backend
        static void* GetActiveWindow();
    };
}