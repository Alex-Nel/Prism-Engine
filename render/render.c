#include "render.h"
#include "../core/log_core.h"
#include <stddef.h>



// Forward declerations of backend specific initialization functions
extern Renderer* Headless_Init();
extern Renderer* OpenGL_Init(void* native_window, uint32_t init_width, uint32_t init_height);
// extern Renderer* Vulkan_Init(Render_LoadProcFn load_proc);
// extern Renderer* DirectX_Init(Render_LoadProcFn load_proc);
// extern Renderer* SoftwareRenderer_Init(Render_LoadProcFn load_proc);



// Initializes the backend depending on the API chosen
Renderer* Render_Init(GraphicsAPI api, void* native_window, uint32_t init_width, uint32_t init_height)
{
    switch (api)
    {
        case GRAPHICS_API_OPENGL:
            Log_Info("Initializing OpenGL...");
            return OpenGL_Init(native_window, init_width, init_height);
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