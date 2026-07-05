#include "../render.h"
#include <stdlib.h>
#include <string.h>



// Only valid-looking IDs are neededso other modules don't panic.
// Just increment a counter every time a resource is "created".
typedef struct Headless_Backend
{
    uint32_t resource_counter;
} Headless_Backend;


// --- No-Op Functions ---

static void Headless_Shutdown(Renderer* r)
{
    free(r->backend_internal_data);
    free(r); 
}

static void Headless_SetViewport(Renderer* r, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {}
static void Headless_SetClearColor(Renderer* r, float red, float green, float blue, float alpha) {}
static void Headless_Clear(Renderer* r) {}

static void Headless_BeginShadowPass(Renderer* r, const RenderPacket* packet) {}
static void Headless_EndShadowPass(Renderer* r) {}

static void Headless_BeginFrame(Renderer* r, const RenderPacket* packet) {}
static void Headless_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat, Matrix4 transform, Matrix4* bone_matrices, bool is_transparent, float depth_distance, bool cast_shadows, bool receive_shadows) {}
static void Headless_EndFrame(Renderer* r) {}


// --- Fake Resource Creators ---

static MeshHandle Headless_CreateMesh(Renderer* r, const Vertex3D* v, uint32_t vc, const uint32_t* i, uint32_t ic)
{
    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    return (MeshHandle){ ++internal->resource_counter };
}

static void Headless_DestroyMesh(Renderer* r, MeshHandle mesh) {}



static TextureHandle Headless_CreateTexture(Renderer* r, const uint8_t* p, uint32_t w, uint32_t h, uint32_t c)
{
    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    return (TextureHandle){ ++internal->resource_counter };
}

static void Headless_DestroyTexture(Renderer* r, TextureHandle texture) {}



static ShaderHandle Headless_CreateShader(Renderer* r, const char* vs, const char* fs)
{
    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    return (ShaderHandle){ ++internal->resource_counter };
}

static void Headless_DestroyShader(Renderer* r, ShaderHandle shader) {}



static TextureHandle Headless_CreateCubemap(Renderer* r, const uint8_t* right, const uint8_t* left, const uint8_t* top, const uint8_t* bottom, const uint8_t* front, const uint8_t* back, uint32_t width, uint32_t height, uint32_t channels)
{
    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    return (TextureHandle){ ++internal->resource_counter };
}



static MeshHandle Headless_CreateDynamicMesh(Renderer* r, uint32_t max_vertices, uint32_t max_indices)
{
    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    return (MeshHandle){ ++internal->resource_counter };
}



static void Headless_UpdateDynamicMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count) {}



// --- Headless initialization Function ---
Renderer* Headless_Init()
{
    Renderer* r = malloc(sizeof(Renderer));
    Headless_Backend* internal = malloc(sizeof(Headless_Backend));
    
    internal->resource_counter = 1; // Start at 1, since 0 is usually "Invalid"
    
    r->backend_internal_data = internal;
    r->api = GRAPHICS_API_NONE;

    // Map all the dummy functions
    r->Shutdown = Headless_Shutdown;
    r->SetViewport = Headless_SetViewport;
    r->SetClearColor = Headless_SetClearColor;
    r->Clear = Headless_Clear;
    
    r->CreateMesh = Headless_CreateMesh;
    r->DestroyMesh = Headless_DestroyMesh;
    r->CreateTexture = Headless_CreateTexture;
    r->DestroyTexture = Headless_DestroyTexture;
    r->CreateShader = Headless_CreateShader;
    r->DestroyShader = Headless_DestroyShader;

    r->CreateCubemap = Headless_CreateCubemap;
    r->CreateDynamicMesh = Headless_CreateDynamicMesh;
    r->UpdateDynamicMesh = Headless_UpdateDynamicMesh;

    r->BeginShadowPass = Headless_BeginShadowPass;
    r->EndShadowPass = Headless_EndShadowPass;

    r->BeginFrame = Headless_BeginFrame;
    r->Submit = Headless_Submit;
    r->EndFrame = Headless_EndFrame;

    return r;
}