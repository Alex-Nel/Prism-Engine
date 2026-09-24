#pragma once

#include <string>
#include <cstdint>
#include "scene/Scene.hpp"
#include "scene/ScriptRegistry.hpp"
#include "core/Math.hpp"
#include "core/Input.hpp"
#include "PrismAPI.hpp"



namespace Prism
{

    // Enum for graphics API's
    enum class GraphicsAPI 
    {
        OPENGL,
        VULKAN,
        DIRECTX,
        SOFTWARE,
        NONE
    };



    struct RendererSettings
    {
        bool enable_ssao = true;               // Enable or disable screen space ambient occlusion
        bool enable_shadows = true;            // Enable or disable shadow mapping
        bool enable_lighting = true;           // Enable or disable the deferred lighting pass
        bool enable_skybox = true;             // Enable or disable skybox rendering
        bool enable_transparency = true;       // Enable or disable forward transparent pass
        bool wireframe_mode = false;           // Render opaque geometry in wireframe mode

        uint32_t shadow_map_resolution = 2048;
        float gamma = 2.2f;

        float exposure = 1.0f;
        uint32_t max_draw_items = 0;
        uint32_t max_shadow_cascades = 0;
        uint32_t max_reflection_probes = 0;
    };



    class PRISM_API Engine
    {
    public:
        // Prevent instantiation
        Engine() = delete;


        // --- Lifecycle ---

        static bool Init(const std::string& title, uint32_t width, uint32_t height, uint32_t target_fps = 60, GraphicsAPI api = GraphicsAPI::OPENGL);
        static void Run(Scene& active_scene);
        static void Shutdown();


        // --- Utility ---

        static void CaptureMouse();
        static void ReleaseMouse();
        static bool IsMouseCaptured();
        static void SetTargetFPS(uint32_t fps);
        static uint32_t GetTargetFPS();
        static void SetVSync(bool enabled);
        static void SetRendererSettings(const RendererSettings& settings);
        static RendererSettings GetRendererSettings();



#ifdef PRISM_EDITOR

        // --- Editor API ---

        static void Update(Scene& active_scene);
        static void Render(Scene& active_scene);
        static bool IsRunning();
        static void SetSimulationMode(bool is_simulating);
        static void SetModalCallback(void (*callback)(void*), void* userdata);

#endif

    };

}