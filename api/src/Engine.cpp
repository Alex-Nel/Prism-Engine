#include "../include/Engine.hpp"


extern "C"
{
    #include "../../runtime/engine_runtime.h"
}


namespace Prism
{
    // Global Engine instance for the API
    static ::PrismEngine s_engine = {};

    // --- LIFECYCLE ---

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



    // --- UTILITY ---

    int Engine::GetWindowWidth() {
        return ::Platform_GetWindowWidth(::Engine_GetMainWindow(&s_engine));
    }

    int Engine::GetWindowHeight() {
        return ::Platform_GetWindowHeight(::Engine_GetMainWindow(&s_engine));
    }

    void Engine::SetClearColor(const Prism::Vector3& color, float alpha) {
        ::Engine_SetClearColor(&s_engine, color.x, color.y, color.z, alpha);
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

    bool Engine::UIWantsMouse() {
        return ::Engine_UIWantsMouse(&s_engine);
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
        c_settings.enable_ssao = settings.enable_ssao;
        c_settings.shadow_map_resolution = settings.shadow_map_resolution;
        c_settings.gamma = settings.gamma;
        ::Render_SetSettings(r, &c_settings);
    }

    Prism::RendererSettings Engine::GetRendererSettings() {
        ::Renderer* r = ::Engine_GetRenderer(&s_engine);
        ::RendererSettings c_settings = ::Render_GetSettings(r);
        Prism::RendererSettings settings;
        settings.enable_ssao = c_settings.enable_ssao;
        settings.shadow_map_resolution = c_settings.shadow_map_resolution;
        settings.gamma = c_settings.gamma;
        return settings;
    }
}