#include "render.h"
#include "../core/log.h"
#include <stddef.h>



// Forward declerations of backend specific initialization functions
extern Renderer* OpenGL_Init(Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height);
// extern Renderer* Vulkan_Init(Render_LoadProcFn load_proc);
// extern Renderer* DirectX_Init(Render_LoadProcFn load_proc);
extern Renderer* Headless_Init();
// extern Renderer* SoftwareRenderer_Init(Render_LoadProcFn load_proc);



// Initializes the backend depending on the API chosen
Renderer* Render_Init(GraphicsAPI api, Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height)
{
    Log_Info("API Chosen: %d", api);
    
    switch (api)
    {
        case GRAPHICS_API_OPENGL:
            Log_Info("Initializing OpenGL...");
            return OpenGL_Init(load_proc, init_width, init_height);
        case GRAPHICS_API_VULKAN:
            Log_Info("Vulkan not implemented yet");
            return NULL;
        case GRAPHICS_API_DIRECTX:
            Log_Info("DirectX not implemented yet");
            return NULL;
        case GRAPHICS_API_NONE:
            Log_Info("Initializing Headless mode...");
            return Headless_Init();
        default:
            Log_Info("Default renderer not implemented yet");
            return NULL;
    }
}