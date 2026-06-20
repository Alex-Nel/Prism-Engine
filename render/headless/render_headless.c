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

static void Headless_BeginFrame(Renderer* r, const RenderPacket* packet) {}
static void Headless_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat, Matrix4 transform, Matrix4* bone_matrices) {}
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

    r->BeginFrame = Headless_BeginFrame;
    r->Submit = Headless_Submit;
    r->EndFrame = Headless_EndFrame;

    return r;
}