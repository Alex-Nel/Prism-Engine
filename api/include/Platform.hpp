#pragma once



namespace Prism
{

    // --- Class for platform functions ---
    class Platform
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
        static void WarpMouse(void* window);

        // Future implementation
        // static void* GetActiveWindow();
    };
}