#pragma once

extern "C"
{
    #include "platform/platformCore.h"
    
    // Note: Since the engine manages the window, the user needs a way 
    // to get the active window pointer from the core engine.
    // Implement that later
}

namespace Prism {

    class Platform {
    public:
        // Prevent accidental instantiation
        Platform() = delete;

        // --- Time ---

        // Gets the high-resolution time since the engine started
        static double GetTime() {
            return ::Platform_GetTime();
        }

        // --- Window & Mouse Management ---

        // Locks the mouse to the window and hides the cursor (Perfect for 3D cameras)
        static void SetRelativeMouseMode(::Window* window, bool enabled) {
            ::Platform_SetRelativeMouseMode(window, enabled);
        }

        // Forces the mouse cursor to the center of the screen
        static void WarpMouse(::Window* window) {
            ::Platform_WarpMouse(window);
        }
    };
}