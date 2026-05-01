#pragma once

#include <string>
#include <cstdint>
#include "scene/Scene.hpp"
#include "core/Math.hpp"



namespace Prism
{

    // Enum for graphics API's
    enum GraphicsAPI 
    {
        GRAPHICS_API_OPENGL,
        GRAPHICS_API_VULKAN,
        GRAPHICS_API_DIRECTX,
        GRAPHICS_API_NONE
    };



    class Engine
    {
    public:
        // Prevent instantiation
        Engine() = delete;


        // --- LIFECYCLE ---

        static bool Init(const std::string& title, uint32_t width, uint32_t height, uint32_t target_fps = 60, GraphicsAPI api = GRAPHICS_API_OPENGL);
        static void Run(Scene& active_scene);
        static void Shutdown();


        // --- UTILITY ---

        static void SetClearColor(const Prism::Vector3& color, float alpha = 1.0f);
        static void CaptureMouse();
        static void ReleaseMouse();
        static bool IsMouseCaptured();
    };
}