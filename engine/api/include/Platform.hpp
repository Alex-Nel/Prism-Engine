#pragma once

#include "PrismAPI.hpp"
#include "core/Image.hpp"
#include <string>
#include <cstdint>



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

        // Sets the window icon from an image file path
        static bool SetWindowIcon(const std::string& filepath);

        // Sets the window icon from an Image object
        static bool SetWindowIcon(const Image& image);

        // Sets the window icon from raw pixel data
        static bool SetWindowIcon(const void* pixels, uint32_t width, uint32_t height, uint32_t channels = 4);

    private:
        // Gets the active window pointer from the backend
        static void* GetActiveWindow();
    };
}