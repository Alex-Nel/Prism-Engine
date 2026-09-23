#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>

#include "../core/graphics_core.h"
#include "../core/math_core.h"
#include "../core/mesh_core.h"
#include "../core/log_core.h"
#include "../core/io_core.h"
#include "../core/overlay_core.h"
#include "../core/frustum_core.h"
#include "render_structures.h"




// Optional runtime hook that forwards a renderer command to its chosen execution context
typedef bool (*RenderCommandDispatchFunction)(void* user_data, RenderCommandType type, const void* input, void* output);

// Optional runtime hook that reports whether direct backend access is currently safe
typedef bool (*RenderDirectAccessFunction)(void* user_data);



typedef struct Renderer Renderer;
typedef struct RenderFrame RenderFrame;



// ------------------------
// --- Renderer V-Table ---
// ------------------------

typedef struct Renderer
{
    GraphicsAPI api;

    // --- Lifecycle ---
    
    void (*Shutdown)(Renderer* r);
    void (*Resize)(Renderer* r, uint32_t width, uint32_t height);
    void (*Present)(Renderer* r);
    void (*SetVSync)(Renderer* r, bool enabled);
    bool (*MakeCurrent)(Renderer* r);
    void (*ReleaseCurrent)(Renderer* r);



    // --- Resource Management ---

    MeshHandle     (*CreateMesh)(Renderer* r, const RenderMeshDesc* desc);
    void           (*UpdateMesh)(Renderer* r, MeshHandle handle, const RenderMeshUpdate* update);
    void           (*DestroyMesh)(Renderer* r, MeshHandle mesh);
    
    TextureHandle  (*CreateTexture)(Renderer* r, const RenderTextureDesc* desc);
    void           (*DestroyTexture)(Renderer* r, TextureHandle texture);

    ShaderHandle   (*CreateShader)(Renderer* r, const RenderShaderDesc* desc);
    void           (*DestroyShader)(Renderer* r, ShaderHandle shader);

    MaterialHandle (*CreateMaterial)(Renderer* r, const RenderMaterialDesc* desc);
    void           (*UpdateMaterial)(Renderer* r, MaterialHandle handle, const RenderMaterialDesc* desc);
    void           (*DestroyMaterial)(Renderer* r, MaterialHandle handle);

    EnvironmentMapHandle (*CreateEnvironmentMap)(Renderer* r, const RenderEnvironmentMapDesc* desc);
    void                 (*DestroyEnvironmentMap)(Renderer* r, EnvironmentMapHandle handle);



    // --- Command Submission ---

    void (*DrawWorld)(Renderer* r, const RenderWorld* world);
    void (*DrawFrame)(Renderer* r, const RenderFrame* frame);
    uint32_t (*GetProbeResults)(Renderer* r, RenderProbeResult* out, uint32_t max_count);



    // --- Overlay Rendering ---

    void (*DrawOverlay)(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height);



    // --- Settings ---

    void (*SetSettings)(Renderer* r, const RendererSettings* settings);
    RendererSettings (*GetSettings)(Renderer* r);



    // --- Hidden implementation-specific data ---
    void* backend_internal_data;
    RenderCommandDispatchFunction command_dispatch;
    RenderDirectAccessFunction direct_access_check;
    void* command_dispatch_data;

} Renderer;





// Function pointer for loading graphics API procedures
typedef void* (*Render_LoadProcFn)(const char* name);

// Initializes a renderer bound to a native window handle
Renderer* Render_Init(GraphicsAPI api, void* native_window, uint32_t init_width, uint32_t init_height);

// Installs optional runtime-owned command forwarding without coupling the renderer to threading
void Render_SetCommandDispatch(Renderer* r, RenderCommandDispatchFunction dispatch, RenderDirectAccessFunction direct_access_check, void* user_data);

// Removes runtime-owned command forwarding callbacks
void Render_ClearCommandDispatch(Renderer* r);

// Returns whether the calling thread may directly execute rendering functions
bool Render_HasDirectAccess(Renderer* r);

// Gives an installed runtime dispatcher a chance to handle a renderer command
bool Render_TryDispatchCommand(Renderer* r, RenderCommandType type, const void* input, void* output);















// ----------------------------------------------
// ----- Wrapper functions for the renderer -----
// ----------------------------------------------



// Shuts down the renderer
static inline void Render_Shutdown(Renderer* r)
{
    if (r && r->Shutdown)
        r->Shutdown(r);
}

// Resizes backend targets (G-buffer, SSAO, lighting). Call on window resize.
static inline void Render_Resize(Renderer* r, uint32_t width, uint32_t height)
{
    RenderResizeCommand command = {width, height};
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_RESIZE, &command, NULL))
        return;
    if (r && r->Resize)
        r->Resize(r, width, height);
}

static inline void Render_Present(Renderer* r)
{
    if (r && Render_HasDirectAccess(r) && r->Present)
        r->Present(r);
}

static inline void Render_SetVSync(Renderer* r, bool enabled)
{
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_SET_VSYNC, &enabled, NULL))
        return;
    if (r && r->SetVSync)
        r->SetVSync(r, enabled);
}

static inline bool Render_MakeCurrent(Renderer* r)
{
    if (r && r->MakeCurrent)
        return r->MakeCurrent(r);
    return false;
}

static inline void Render_ReleaseCurrent(Renderer* r)
{
    if (r && r->ReleaseCurrent)
        r->ReleaseCurrent(r);
}





// Returns the pixel format enum based on the number of channels
static inline RenderPixelFormat Render_PixelFormatFromChannels(uint32_t channels)
{
    if (channels == 1)
        return RENDER_FORMAT_R8;
    if (channels == 2)
        return RENDER_FORMAT_RG8;
    if (channels == 3)
        return RENDER_FORMAT_RGB8;
    return RENDER_FORMAT_RGBA8;
}





// Uploads vertex and index data to the GPU and returns a handle
static inline MeshHandle Render_CreateMesh(Renderer* r, const RenderMeshDesc* desc)
{
    MeshHandle result = {0};
    if (r && desc && Render_TryDispatchCommand(r, RENDER_COMMAND_CREATE_MESH, desc, &result))
        return result;
    if (r && r->CreateMesh && desc)
        return r->CreateMesh(r, desc);
    MeshHandle invalid = {0};
    return invalid;
}
static inline MeshHandle Render_CreateStaticMesh(Renderer* r, const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    RenderMeshDesc desc = {};
    desc.vertex_format = RENDER_VERTEX_STATIC;
    desc.usage = RENDER_MESH_STATIC;
    desc.vertices = vertices;
    desc.vertex_count = vertex_count;
    desc.indices = indices;
    desc.index_count = index_count;
    return Render_CreateMesh(r, &desc);
}
static inline MeshHandle Render_CreateSkinnedMesh(Renderer* r, const Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    RenderMeshDesc desc = {};
    desc.vertex_format = RENDER_VERTEX_SKINNED;
    desc.usage = RENDER_MESH_STATIC;
    desc.vertices = vertices;
    desc.vertex_count = vertex_count;
    desc.indices = indices;
    desc.index_count = index_count;
    return Render_CreateMesh(r, &desc);
}
static inline MeshHandle Render_CreateDynamicMesh(Renderer* r, uint32_t max_vertices, uint32_t max_indices)
{
    RenderMeshDesc desc = {};
    desc.vertex_format = RENDER_VERTEX_STATIC;
    desc.usage = RENDER_MESH_DYNAMIC;
    desc.max_vertices = max_vertices;
    desc.max_indices = max_indices;
    return Render_CreateMesh(r, &desc);
}
static inline void Render_UpdateMesh(Renderer* r, MeshHandle handle, const RenderMeshUpdate* update)
{
    RenderMeshUpdateCommand command = {handle, update};
    if (r && update && Render_TryDispatchCommand(r, RENDER_COMMAND_UPDATE_MESH, &command, NULL))
        return;
    if (r && r->UpdateMesh && update)
        r->UpdateMesh(r, handle, update);
}
// Removes a mesh from the GPU
static inline void Render_DestroyMesh(Renderer* r, MeshHandle mesh)
{
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_DESTROY_MESH, &mesh, NULL))
        return;
    if (r && r->DestroyMesh)
        r->DestroyMesh(r, mesh);
}





// Uploads pixels to the renderer to make a texture. Returns a handle
static inline TextureHandle Render_CreateTexture(Renderer* r, const RenderTextureDesc* desc)
{
    TextureHandle result = {0};
    if (r && desc && Render_TryDispatchCommand(r, RENDER_COMMAND_CREATE_TEXTURE, desc, &result))
        return result;
    if (r && r->CreateTexture && desc)
        return r->CreateTexture(r, desc);
    TextureHandle invalid = {0};
    return invalid;
}
static inline TextureHandle Render_CreateTexture2D(Renderer* r, const void* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    RenderTextureDesc desc = {};
    desc.type = RENDER_TEXTURE_2D;
    desc.format = Render_PixelFormatFromChannels(channels);
    desc.width = width;
    desc.height = height;
    desc.pixels = pixels;
    return Render_CreateTexture(r, &desc);
}
static inline TextureHandle Render_CreateCubemap(Renderer* r, const uint8_t* right, const uint8_t* left, const uint8_t* top, const uint8_t* bottom, const uint8_t* front, const uint8_t* back, uint32_t width, uint32_t height, uint32_t channels)
{
    RenderTextureDesc desc = {};
    desc.type = RENDER_TEXTURE_CUBE;
    desc.format = Render_PixelFormatFromChannels(channels);
    desc.width = width;
    desc.height = height;
    desc.cube_faces[0] = right;
    desc.cube_faces[1] = left;
    desc.cube_faces[2] = top;
    desc.cube_faces[3] = bottom;
    desc.cube_faces[4] = front;
    desc.cube_faces[5] = back;
    return Render_CreateTexture(r, &desc);
}
// Removes a texture from the GPU
static inline void Render_DestroyTexture(Renderer* r, TextureHandle texture)
{
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_DESTROY_TEXTURE, &texture, NULL))
        return;
    if (r && r->DestroyTexture)
        r->DestroyTexture(r, texture);
}





// Uploads a shader program. Returns a handle
static inline ShaderHandle Render_CreateShader(Renderer* r, const RenderShaderDesc* desc)
{
    ShaderHandle result = {0};
    if (r && desc && Render_TryDispatchCommand(r, RENDER_COMMAND_CREATE_SHADER, desc, &result))
        return result;
    if (r && r->CreateShader && desc)
        return r->CreateShader(r, desc);
    ShaderHandle invalid = {0};
    return invalid;
}
static inline ShaderHandle Render_CreateShaderGLSL(Renderer* r, const char* vertex_source, const char* fragment_source)
{
    RenderShaderDesc desc = {};
    desc.format = RENDER_SHADER_GLSL_SOURCE;
    desc.vertex_code = vertex_source;
    desc.fragment_code = fragment_source;
    return Render_CreateShader(r, &desc);
}
// Removes a shader from the GPU
static inline void Render_DestroyShader(Renderer* r, ShaderHandle shader)
{
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_DESTROY_SHADER, &shader, NULL))
        return;
    if (r && r->DestroyShader)
        r->DestroyShader(r, shader);
}





// Creates a GPU material from a CPU description
static inline MaterialHandle Render_CreateMaterial(Renderer* r, const RenderMaterialDesc* desc)
{
    MaterialHandle result = {0};
    if (r && desc && Render_TryDispatchCommand(r, RENDER_COMMAND_CREATE_MATERIAL, desc, &result))
        return result;
    if (r && r->CreateMaterial && desc)
        return r->CreateMaterial(r, desc);
    MaterialHandle invalid = {0};
    return invalid;
}
// Updates an existing GPU material
static inline void Render_UpdateMaterial(Renderer* r, MaterialHandle handle, const RenderMaterialDesc* desc)
{
    RenderMaterialUpdateCommand command = {handle, desc};
    if (r && desc && Render_TryDispatchCommand(r, RENDER_COMMAND_UPDATE_MATERIAL, &command, NULL))
        return;
    if (r && r->UpdateMaterial && desc)
        r->UpdateMaterial(r, handle, desc);
}
// Removes a GPU material
static inline void Render_DestroyMaterial(Renderer* r, MaterialHandle handle)
{
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_DESTROY_MATERIAL, &handle, NULL))
        return;
    if (r && r->DestroyMaterial)
        r->DestroyMaterial(r, handle);
}





// Creates GPU IBL resources (or wraps a cubemap as a skybox-only environment). Returns a handle
static inline EnvironmentMapHandle Render_CreateEnvironmentMap(Renderer* r, const RenderEnvironmentMapDesc* desc)
{
    EnvironmentMapHandle result = {0};
    if (r && desc && Render_TryDispatchCommand(r, RENDER_COMMAND_CREATE_ENVIRONMENT, desc, &result))
        return result;
    if (r && r->CreateEnvironmentMap && desc)
        return r->CreateEnvironmentMap(r, desc);
    EnvironmentMapHandle invalid = {0};
    return invalid;
}
static inline void Render_DestroyEnvironmentMap(Renderer* r, EnvironmentMapHandle handle)
{
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_DESTROY_ENVIRONMENT, &handle, NULL))
        return;
    if (r && r->DestroyEnvironmentMap)
        r->DestroyEnvironmentMap(r, handle);
}










// Draws a complete view snapshot. Backends must implement DrawWorld.
static inline void Render_DrawWorld(Renderer* r, const RenderWorld* world)
{
    if (r && Render_HasDirectAccess(r) && r->DrawWorld && world)
        r->DrawWorld(r, world);
}

// Draws a scene into a render frame
static inline void Render_DrawFrame(Renderer* r, const RenderFrame* frame)
{
    if (r && Render_HasDirectAccess(r) && r->DrawFrame && frame)
        r->DrawFrame(r, frame);
}

// Copies probe capture results from the last DrawWorld. If out is NULL, returns the available count.
static inline uint32_t Render_GetProbeResults(Renderer* r, RenderProbeResult* out, uint32_t max_count)
{
    uint32_t count = 0;
    RenderProbeResultsCommand command = {out, max_count};
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_GET_PROBE_RESULTS, &command, &count))
        return count;
    if (r && r->GetProbeResults)
        return r->GetProbeResults(r, out, max_count);
    return 0;
}










// Renders any Overlay
static inline void Render_DrawOverlay(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height)
{
    if (r && Render_HasDirectAccess(r) && r->DrawOverlay)
        r->DrawOverlay(r, list, width, height);
}










// Sets all renderer settings according to the specified struct
static inline void Render_SetSettings(Renderer* r, const RendererSettings* settings)
{
    if (r && settings && Render_TryDispatchCommand(r, RENDER_COMMAND_SET_SETTINGS, settings, NULL))
        return;
    if (r && r->SetSettings && settings)
        r->SetSettings(r, settings);
}

// Returns all the settings of the renderer
static inline RendererSettings Render_GetSettings(Renderer* r)
{
    RendererSettings dispatched = {0};
    if (r && Render_TryDispatchCommand(r, RENDER_COMMAND_GET_SETTINGS, NULL, &dispatched))
        return dispatched;
    if (r && r->GetSettings)
        return r->GetSettings(r);
    RendererSettings empty = {0};
    return empty;
}

// Sets the gamma value of the renderer
static inline void Render_SetGamma(Renderer* r, float gamma)
{
    if (!r)
        return;
    RendererSettings s = Render_GetSettings(r);
    s.gamma = gamma;
    if (gamma < 0.1f)
        s.gamma = 0.1f;
    Render_SetSettings(r, &s);
}

// Sets the resolution of the renderers shadow map
static inline void Render_SetShadowMapResolution(Renderer* r, uint32_t resolution)
{
    if (!r)
        return;
    if (resolution < 256)
        resolution = 256;
    if (resolution > 8192)
        resolution = 8192;
    RendererSettings s = Render_GetSettings(r);
    s.shadow_map_resolution = resolution;
    Render_SetSettings(r, &s);
}

// Sets whether Screen Space Ambient Occlusion is enabled or not
static inline void Render_SetSSAOEnabled(Renderer* r, bool enabled)
{
    if (!r)
        return;
    RendererSettings s = Render_GetSettings(r);
    s.enable_ssao = enabled;
    Render_SetSettings(r, &s);
}





#endif