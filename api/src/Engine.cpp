#include "../include/Engine.hpp"


extern "C"
{
    #include "../../Prism.h"
}


namespace Prism
{
    // --- LIFECYCLE ---

    bool Engine::Init(const std::string& title, uint32_t width, uint32_t height, uint32_t target_fps, GraphicsAPI api) 
    {
        // Safely cast our C++ enum back to the C backend's enum type
        return ::Engine_Init(title.c_str(), width, height, target_fps, static_cast<::GraphicsAPI>(api));
    }

    void Engine::Run(Scene& active_scene) 
    {
        // Grab the opaque void* from the C++ Scene and cast it back to the raw C struct!
        ::Scene* raw_scene = static_cast<::Scene*>(active_scene.GetRaw());
        ::Engine_Run(raw_scene);
    }

    void Engine::Shutdown() 
    {
        ::Engine_Shutdown();
    }


    // --- UTILITY ---

    void Engine::SetClearColor(const Prism::Vector3& color, float alpha) 
    {
        ::Render_SetClearColor(color.x, color.y, color.z, alpha);
    }

    void Engine::CaptureMouse() 
    {
        ::Engine_CaptureMouse();
    }

    void Engine::ReleaseMouse() 
    {
        ::Engine_ReleaseMouse();
    }

    bool Engine::IsMouseCaptured() 
    {
        return ::Engine_IsMouseCaptured();
    }
}