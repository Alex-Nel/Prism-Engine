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
        ::Engine_Run(&s_engine, raw_scene);
    }

    void Engine::Shutdown() 
    {
        Prism::ScriptRegistry::Clear();
        ::Engine_Shutdown(&s_engine);
    }



    // --- Utility ---

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
        ::Platform_SetVSync(enabled);
    }

    void Engine::SetRendererSettings(const Prism::RendererSettings& settings) {
        ::Renderer* r = ::Engine_GetRenderer(&s_engine);
        ::RendererSettings c_settings;
        std::memset(&c_settings, 0, sizeof(c_settings));
        c_settings.enable_ssao = settings.enable_ssao;
        c_settings.shadow_map_resolution = settings.shadow_map_resolution;
        c_settings.gamma = settings.gamma;
        c_settings.exposure = settings.exposure;
        c_settings.max_draw_items = settings.max_draw_items;
        c_settings.max_shadow_cascades = settings.max_shadow_cascades;
        c_settings.max_reflection_probes = settings.max_reflection_probes;
        ::Render_SetSettings(r, &c_settings);
    }

    Prism::RendererSettings Engine::GetRendererSettings() {
        ::Renderer* r = ::Engine_GetRenderer(&s_engine);
        ::RendererSettings c_settings = ::Render_GetSettings(r);
        Prism::RendererSettings settings;
        settings.enable_ssao = c_settings.enable_ssao;
        settings.shadow_map_resolution = c_settings.shadow_map_resolution;
        settings.gamma = c_settings.gamma;
        settings.exposure = c_settings.exposure;
        settings.max_draw_items = c_settings.max_draw_items;
        settings.max_shadow_cascades = c_settings.max_shadow_cascades;
        settings.max_reflection_probes = c_settings.max_reflection_probes;
        return settings;
    }



    void* Platform::GetActiveWindow() {
        return ::Engine_GetMainWindow(&s_engine);
    }





#ifdef PRISM_EDITOR

    void Engine::Update(Scene& active_scene) {
        ::Scene* raw_scene = static_cast<::Scene*>(active_scene.GetRaw());
        ::Engine_Update(&s_engine, raw_scene);
    }

    void Engine::Render(Scene& active_scene) {
        ::Scene* raw_scene = static_cast<::Scene*>(active_scene.GetRaw());
        ::Engine_Render(&s_engine, raw_scene);
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