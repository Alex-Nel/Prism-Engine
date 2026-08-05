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
        static void SetRelativeMouseMode(void* window, bool enabled);

        // Forces the mouse cursor to the center of the screen
        static void WarpMouseToMiddle(void* window);

        // Copies UTF-8 text to the system clipboard
        static bool SetClipboardText(const std::string& text);

        // Gets UTF-8 text from the system clipboard
        static std::string GetClipboardText();

        // Future implementation
        // static void* GetActiveWindow();
    };
}