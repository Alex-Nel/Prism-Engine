#pragma once

#include <string>
#include "scene/Scene.hpp"

extern "C"
{
    #include "Prism.h"
}

namespace Prism
{
    class Engine
    {
    public:
        // Prevent instantiation
        Engine() = delete;

        // --- LIFECYCLE ---

        static bool Init(const std::string& title, uint32_t width, uint32_t height, uint32_t target_fps = 60, GraphicsAPI api = ::GRAPHICS_API_OPENGL) {
            return ::Engine_Init(title.c_str(), width, height, target_fps, api);
        }

        static void Run(Scene& active_scene) {
            // Get the raw scene pointer and pass it to the loop
            ::Engine_Run(active_scene.GetRaw());
        }

        static void Shutdown() {
            ::Engine_Shutdown();
        }



        // --- UTILITY ---

        static void SetClearColor(const Prism::Vector3& color, float alpha = 1.0f) {
            ::Render_SetClearColor(color.x, color.y, color.z, alpha);
        }

        static void CaptureMouse() {
            ::Engine_CaptureMouse();
        }

        static void ReleaseMouse() {
            ::Engine_ReleaseMouse();
        }

        static bool IsMouseCaptured() {
            return ::Engine_IsMouseCaptured();
        }
    };
}