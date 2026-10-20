#include "../include/Platform.hpp"
#include "../include/Engine.hpp"


extern "C"
{
    #include "../../runtime/engine_runtime.h"
    #include "cstring"
}


namespace Prism
{
    // Global Engine instance for the API
    static ::PrismEngine s_engine = {};



    // --- Lifecycle ---

    bool Engine::Init(const std::string& title, uint32_t width, uint32_t height, uint32_t target_fps, GraphicsAPI api) 
    {
        // Safely cast our C++ enum back to the C backend's enum type
        return ::Engine_Init(&s_engine, title.c_str(), width, height, target_fps, static_cast<::GraphicsAPI>(api));
    }

    void Engine::Run(Scene& active_scene) 
    {
        ::Engine_SetPreUpdateCallback(&s_engine, &Prism::Input::DispatchCallbacks);
        ::Scene* raw_scene = static_cast<::Scene*>(active_scene.GetRaw());
        ::Engine_SetActiveScene(&s_engine, raw_scene);
        ::Engine_Run(&s_engine);
    }

    void Engine::Shutdown() 
    {
        Prism::ScriptRegistry::Clear();
        ::Engine_Shutdown(&s_engine);
    }



    // --- Utility ---

    void Engine::SetActiveScene(Prism::Scene scene) {
        ::Engine_SetActiveScene(&s_engine, static_cast<::Scene*>(scene.GetRaw()));
    }

    void Engine::CaptureMouse() {
        ::Engine_CaptureMouse(&s_engine);
    }

    void Engine::ReleaseMouse() {
        ::Engine_ReleaseMouse(&s_engine);
    }

    bool Engine::IsMouseCaptured() {
        return ::Engine_IsMouseCaptured(&s_engine);
    }

    void Engine::SetTargetFPS(uint32_t fps) {
        ::Engine_SetTargetFPS(&s_engine, fps);
    }

    uint32_t Engine::GetTargetFPS() {
        return ::Engine_GetTargetFPS(&s_engine);
    }

    void Engine::SetVSync(bool enabled) {
        ::Render_SetVSync(::Engine_GetRenderer(&s_engine), enabled);
    }

    void Engine::SetRendererSettings(const Prism::RendererSettings& settings) {
        ::Renderer* r = ::Engine_GetRenderer(&s_engine);
        ::RendererSettings c_settings;
        std::memset(&c_settings, 0, sizeof(c_settings));
        c_settings.enable_ssao = settings.enable_ssao;
        c_settings.enable_shadows = settings.enable_shadows;
        c_settings.enable_lighting = settings.enable_lighting;
        c_settings.enable_skybox = settings.enable_skybox;
        c_settings.enable_transparency = settings.enable_transparency;
        c_settings.wireframe_mode = settings.wireframe_mode;

        c_settings.shadow_map_resolution = settings.shadow_map_resolution;
        c_settings.resolution_scale = settings.resolution_scale;
        c_settings.gamma = settings.gamma;
        c_settings.exposure = settings.exposure;
        c_settings.max_draw_items = settings.max_draw_items;
        c_settings.max_shadow_cascades = settings.max_shadow_cascades;
        c_settings.max_reflection_probes = settings.max_reflection_probes;
        c_settings.max_active_local_lights = settings.max_active_local_lights;

        ::Render_SetSettings(r, &c_settings);
    }

    Prism::RendererSettings Engine::GetRendererSettings() {
        ::Renderer* r = ::Engine_GetRenderer(&s_engine);
        ::RendererSettings c_settings = ::Render_GetSettings(r);
        Prism::RendererSettings settings;
        settings.enable_ssao = c_settings.enable_ssao;
        settings.enable_shadows = c_settings.enable_shadows;
        settings.enable_lighting = c_settings.enable_lighting;
        settings.enable_skybox = c_settings.enable_skybox;
        settings.enable_transparency = c_settings.enable_transparency;
        settings.wireframe_mode = c_settings.wireframe_mode;

        settings.shadow_map_resolution = c_settings.shadow_map_resolution;
        settings.resolution_scale = c_settings.resolution_scale;
        settings.gamma = c_settings.gamma;
        settings.exposure = c_settings.exposure;
        settings.max_draw_items = c_settings.max_draw_items;
        settings.max_shadow_cascades = c_settings.max_shadow_cascades;
        settings.max_reflection_probes = c_settings.max_reflection_probes;
        settings.max_active_local_lights = c_settings.max_active_local_lights;

        return settings;
    }

    Prism::RenderStats Engine::GetRenderStats() {
        ::RenderStats c_stats = ::Engine_GetRenderStats(&s_engine);
        Prism::RenderStats stats;
        stats.frame_id = c_stats.frame_id;
        stats.gpu_timing_frame_id = c_stats.gpu_timing_frame_id;
        stats.view_count = c_stats.view_count;
        stats.items_submitted = c_stats.items_submitted;
        stats.opaque_items = c_stats.opaque_items;
        stats.transparent_items = c_stats.transparent_items;
        stats.draw_calls = c_stats.draw_calls;
        stats.triangles = c_stats.triangles;
        stats.frustum_culled_items = c_stats.frustum_culled_items;
        stats.local_lights_considered = c_stats.local_lights_considered;
        stats.active_point_lights = c_stats.active_point_lights;
        stats.active_spot_lights = c_stats.active_spot_lights;
        stats.dropped_local_lights = c_stats.dropped_local_lights;
        for (uint32_t i = 0; i < static_cast<uint32_t>(Prism::RenderTimedPass::Count); i++) {
            stats.gpu_time_ms[i] = c_stats.gpu_time_ms[i];
            stats.gpu_time_valid[i] = c_stats.gpu_time_valid[i];
        }
        return stats;
    }



    void* Platform::GetActiveWindow() {
        return ::Engine_GetMainWindow(&s_engine);
    }





#ifdef PRISM_EDITOR

    void Engine::Update(Scene& active_scene) {
        // ::Scene* raw_scene = static_cast<::Scene*>(active_scene.GetRaw());
        ::Engine_Update(&s_engine);
    }

    void Engine::Render(Scene& active_scene) {
        // ::Scene* raw_scene = static_cast<::Scene*>(active_scene.GetRaw());
        ::Engine_Render(&s_engine);
    }

    bool Engine::IsRunning() {
        return s_engine.is_running;
    }

    void Engine::SetSimulationMode(bool is_simulating) {
        ::Engine_SetSimulationMode(&s_engine, is_simulating);
    }

    void Engine::SetModalCallback(void (*callback)(void*), void* userdata) {
        ::Engine_SetModalCallback(&s_engine, callback, userdata);
    }

#endif

}