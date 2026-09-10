#include "render.h"
#include "../core/log_core.h"
#include "../platform/platform_core.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>



// Forward declerations of backend specific initialization functions
extern Renderer* Headless_Init();
extern Renderer* OpenGL_Init(void* native_window, uint32_t init_width, uint32_t init_height);
// extern Renderer* Vulkan_Init(Render_LoadProcFn load_proc);
// extern Renderer* DirectX_Init(Render_LoadProcFn load_proc);
// extern Renderer* SoftwareRenderer_Init(Render_LoadProcFn load_proc);





typedef struct RenderCommandDispatch
{
    PlatformMutex* mutex;
    PlatformCondition* command_complete;
    RenderThreadWakeFunction wake;
    void* wake_user_data;
    uint64_t render_thread_id;
    RenderCommandType type;
    const void* input;
    void* output;
    bool pending;
    bool stopping;
    uint32_t active_callers;
    RendererSettings cached_settings;
    bool cached_settings_valid;
} RenderCommandDispatch;





// Enables synchronized renderer command forwarding from non-render threads
bool Render_EnableThreadDispatch(Renderer* r, RenderThreadWakeFunction wake, void* wake_user_data)
{
    if (!r || r->command_dispatch_data)
        return false;

    RenderCommandDispatch* dispatch = (RenderCommandDispatch*)calloc(1, sizeof(RenderCommandDispatch));
    if (!dispatch)
        return false;
    
    dispatch->mutex = Platform_CreateMutex();
    dispatch->command_complete = Platform_CreateCondition();
    if (!dispatch->mutex || !dispatch->command_complete)
    {
        if (dispatch->command_complete)
            Platform_DestroyCondition(dispatch->command_complete);
        if (dispatch->mutex)
            Platform_DestroyMutex(dispatch->mutex);

        free(dispatch);
        return false;
    }
    
    dispatch->wake = wake;
    dispatch->wake_user_data = wake_user_data;
    if (r->GetSettings)
    {
        dispatch->cached_settings = r->GetSettings(r);
        dispatch->cached_settings_valid = true;
    }
    r->command_dispatch_data = dispatch;
    
    return true;
}





// Records which platform thread exclusively owns backend rendering calls
void Render_SetRenderThreadID(Renderer* r, uint64_t thread_id)
{
    if (!r || !r->command_dispatch_data)
        return;

    RenderCommandDispatch* dispatch = (RenderCommandDispatch*)r->command_dispatch_data;
    Platform_LockMutex(dispatch->mutex);
    dispatch->render_thread_id = thread_id;
    Platform_UnlockMutex(dispatch->mutex);
}





// Returns whether the calling thread may directly execute rendering functions
bool Render_IsOnRenderThread(Renderer* r)
{
    if (!r || !r->command_dispatch_data)
        return true;

    RenderCommandDispatch* dispatch = (RenderCommandDispatch*)r->command_dispatch_data;
    Platform_LockMutex(dispatch->mutex);
    bool is_owner = dispatch->render_thread_id == Platform_GetCurrentThreadID();
    Platform_UnlockMutex(dispatch->mutex);
    
    return is_owner;
}





// Forwards one renderer command to the render thread and waits for its result
bool Render_TryDispatchCommand(Renderer* r, RenderCommandType type, const void* input, void* output)
{
    if (!r || !r->command_dispatch_data)
        return false;

    RenderCommandDispatch* dispatch = (RenderCommandDispatch*)r->command_dispatch_data;
    Platform_LockMutex(dispatch->mutex);

    // Calls already on the render thread can use the backend vtable directly
    if (dispatch->render_thread_id == Platform_GetCurrentThreadID())
    {
        Platform_UnlockMutex(dispatch->mutex);
        return false;
    }
    
    // Settings reads use the synchronized CPU copy instead of stalling on the backend
    if (type == RENDER_COMMAND_GET_SETTINGS && dispatch->cached_settings_valid)
    {
        *(RendererSettings*)output = dispatch->cached_settings;
        Platform_UnlockMutex(dispatch->mutex);
        return true;
    }
    if (type == RENDER_COMMAND_SET_SETTINGS && input)
    {
        dispatch->cached_settings = *(const RendererSettings*)input;
        dispatch->cached_settings_valid = true;
    }
    
    // Only one borrowed command payload may be pending at a time
    dispatch->active_callers++;
    while (dispatch->pending && !dispatch->stopping)
        Platform_WaitCondition(dispatch->command_complete, dispatch->mutex);

    if (dispatch->stopping)
    {
        dispatch->active_callers--;
        Platform_BroadcastCondition(dispatch->command_complete);
        Platform_UnlockMutex(dispatch->mutex);
        return true;
    }
    
    // Publish the command and wake the thread that owns the graphics context
    dispatch->type = type;
    dispatch->input = input;
    dispatch->output = output;
    dispatch->pending = true;
    if (dispatch->wake)
        dispatch->wake(dispatch->wake_user_data);
    
    // The caller remains blocked so its input and output pointers stay valid
    while (dispatch->pending && !dispatch->stopping)
        Platform_WaitCondition(dispatch->command_complete, dispatch->mutex);
    
    bool handled = !dispatch->stopping;
    bool consumed = handled || dispatch->stopping;
    dispatch->active_callers--;
    Platform_BroadcastCondition(dispatch->command_complete);
    Platform_UnlockMutex(dispatch->mutex);
    
    // A stopping dispatcher still consumes the call so wrappers never fall
    // through to a backend whose context belongs to another thread.
    return consumed;
}





// Executes one pending forwarded command on the render thread
bool Render_ProcessPendingCommand(Renderer* r)
{
    if (!r || !r->command_dispatch_data)
        return false;

    RenderCommandDispatch* dispatch = (RenderCommandDispatch*)r->command_dispatch_data;
    Platform_LockMutex(dispatch->mutex);
    if (!dispatch->pending)
    {
        Platform_UnlockMutex(dispatch->mutex);
        return false;
    }
    
    RenderCommandType type = dispatch->type;
    const void* input = dispatch->input;
    void* output = dispatch->output;
    bool refresh_settings = type == RENDER_COMMAND_SET_SETTINGS;
    Platform_UnlockMutex(dispatch->mutex);

    // Dispatch directly through the vtable since wrapper calls would recurse
    switch (type)
    {
        case RENDER_COMMAND_CREATE_MESH:
            *(MeshHandle*)output = r->CreateMesh(r, (const RenderMeshDesc*)input);
            break;
        case RENDER_COMMAND_UPDATE_MESH:
        {
            const RenderMeshUpdateCommand* command = (const RenderMeshUpdateCommand*)input;
            r->UpdateMesh(r, command->handle, command->update);
            break;
        }
        case RENDER_COMMAND_DESTROY_MESH:
            r->DestroyMesh(r, *(const MeshHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_TEXTURE:
            *(TextureHandle*)output = r->CreateTexture(r, (const RenderTextureDesc*)input);
            break;
        case RENDER_COMMAND_DESTROY_TEXTURE:
            r->DestroyTexture(r, *(const TextureHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_SHADER:
            *(ShaderHandle*)output = r->CreateShader(r, (const RenderShaderDesc*)input);
            break;
        case RENDER_COMMAND_DESTROY_SHADER:
            r->DestroyShader(r, *(const ShaderHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_MATERIAL:
            *(MaterialHandle*)output = r->CreateMaterial(r, (const RenderMaterialDesc*)input);
            break;
        case RENDER_COMMAND_UPDATE_MATERIAL:
        {
            const RenderMaterialUpdateCommand* command = (const RenderMaterialUpdateCommand*)input;
            r->UpdateMaterial(r, command->handle, command->desc);
            break;
        }
        case RENDER_COMMAND_DESTROY_MATERIAL:
            r->DestroyMaterial(r, *(const MaterialHandle*)input);
            break;


        case RENDER_COMMAND_CREATE_ENVIRONMENT:
            *(EnvironmentMapHandle*)output = r->CreateEnvironmentMap(r, (const RenderEnvironmentMapDesc*)input);
            break;
        case RENDER_COMMAND_DESTROY_ENVIRONMENT:
            r->DestroyEnvironmentMap(r, *(const EnvironmentMapHandle*)input);
            break;


        case RENDER_COMMAND_RESIZE:
        {
            const RenderResizeCommand* command = (const RenderResizeCommand*)input;
            r->Resize(r, command->width, command->height);
            break;
        }
        case RENDER_COMMAND_SET_SETTINGS:
            r->SetSettings(r, (const RendererSettings*)input);
            break;
        case RENDER_COMMAND_GET_SETTINGS:
            *(RendererSettings*)output = r->GetSettings(r);
            break;
        case RENDER_COMMAND_SET_VSYNC:
            r->SetVSync(r, *(const bool*)input);
            break;
        case RENDER_COMMAND_GET_PROBE_RESULTS:
        {
            const RenderProbeResultsCommand* command = (const RenderProbeResultsCommand*)input;
            *(uint32_t*)output = r->GetProbeResults(r, command->out, command->max_count);
            break;
        }
    }

    // Publish any result and release the waiting caller
    Platform_LockMutex(dispatch->mutex);
    if (refresh_settings && r->GetSettings)
    {
        dispatch->cached_settings = r->GetSettings(r);
        dispatch->cached_settings_valid = true;
    }

    dispatch->pending = false;
    dispatch->input = NULL;
    dispatch->output = NULL;
    Platform_BroadcastCondition(dispatch->command_complete);
    Platform_UnlockMutex(dispatch->mutex);
    
    return true;
}





// Stops command forwarding and releases its synchronization state
void Render_DisableThreadDispatch(Renderer* r)
{
    if (!r || !r->command_dispatch_data)
        return;

    RenderCommandDispatch* dispatch = (RenderCommandDispatch*)r->command_dispatch_data;
    Platform_LockMutex(dispatch->mutex);
    dispatch->stopping = true;
    Platform_BroadcastCondition(dispatch->command_complete);
    
    while (dispatch->active_callers > 0)
        Platform_WaitCondition(dispatch->command_complete, dispatch->mutex);
    
    Platform_UnlockMutex(dispatch->mutex);
    Platform_DestroyCondition(dispatch->command_complete);
    Platform_DestroyMutex(dispatch->mutex);
    
    free(dispatch);
    r->command_dispatch_data = NULL;
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