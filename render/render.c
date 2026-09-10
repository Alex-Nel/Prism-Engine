#include "render.h"
#include "../core/log_core.h"



// Forward declerations of backend specific initialization functions
extern Renderer* Headless_Init();
extern Renderer* OpenGL_Init(void* native_window, uint32_t init_width, uint32_t init_height);
// extern Renderer* Vulkan_Init(Render_LoadProcFn load_proc);
// extern Renderer* DirectX_Init(Render_LoadProcFn load_proc);
// extern Renderer* SoftwareRenderer_Init(Render_LoadProcFn load_proc);





// Installs optional runtime-owned command forwarding callbacks
void Render_SetCommandDispatch(Renderer* r, RenderCommandDispatchFunction dispatch, RenderDirectAccessFunction direct_access_check, void* user_data)
{
    if (!r)
        return;

    r->command_dispatch = dispatch;
    r->direct_access_check = direct_access_check;
    r->command_dispatch_data = user_data;
}





// Removes runtime-owned command forwarding callbacks
void Render_ClearCommandDispatch(Renderer* r)
{
    if (!r)
        return;

    r->command_dispatch = NULL;
    r->direct_access_check = NULL;
    r->command_dispatch_data = NULL;
}





// Returns whether the caller is allowed to invoke the backend directly
bool Render_HasDirectAccess(Renderer* r)
{
    if (!r || !r->direct_access_check)
        return true;

    return r->direct_access_check(r->command_dispatch_data);
}





// Gives an installed runtime dispatcher a chance to handle a renderer command
bool Render_TryDispatchCommand(Renderer* r, RenderCommandType type, const void* input, void* output)
{
    if (!r || !r->command_dispatch)
        return false;

    return r->command_dispatch(r->command_dispatch_data, type, input, output);
}





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