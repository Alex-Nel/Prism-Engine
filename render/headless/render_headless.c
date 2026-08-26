#include "../render.h"
#include <stdlib.h>
#include <string.h>



// Only valid-looking IDs are neededso other modules don't panic.
// Just increment a counter every time a resource is "created".
typedef struct Headless_Backend
{
    uint32_t resource_counter;
    RendererSettings settings;
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

static void Headless_SetSettings(Renderer* r, const RendererSettings* settings)
{
    if (!r || !r->backend_internal_data || !settings)
        return;

    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    internal->settings.enable_ssao = settings->enable_ssao;
    if (settings->shadow_map_resolution > 0)
        internal->settings.shadow_map_resolution = settings->shadow_map_resolution;
    if (settings->gamma > 0.01f)
        internal->settings.gamma = settings->gamma;
    if (settings->exposure > 0.001f)
        internal->settings.exposure = settings->exposure;
    if (settings->max_draw_items > 0)
        internal->settings.max_draw_items = settings->max_draw_items;
}

static RendererSettings Headless_GetSettings(Renderer* r)
{
    if (!r || !r->backend_internal_data)
    {
        RendererSettings empty = {0};
        return empty;
    }
    return ((Headless_Backend*)r->backend_internal_data)->settings;
}

static void Headless_DrawWorld(Renderer* r, const RenderWorld* world) {}

static void Headless_UIinit(Renderer* r, void* nk_ctx) { (void)r; (void)nk_ctx; }
static void Headless_UIShutdown(Renderer* r) { (void)r; }
static void Headless_UIRender(Renderer* r, void* nk_ctx, uint32_t width, uint32_t height) { (void)r; (void)nk_ctx; (void)width; (void)height; }
static void Headless_DrawOverlay(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height) { (void)r; (void)list; (void)width; (void)height; }


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



static MaterialHandle Headless_CreateMaterial(Renderer* r, const RenderMaterialDesc* desc)
{
    (void)desc;
    Headless_Backend* internal = (Headless_Backend*)r->backend_internal_data;
    return (MaterialHandle){ ++internal->resource_counter };
}

static void Headless_UpdateMaterial(Renderer* r, MaterialHandle handle, const RenderMaterialDesc* desc) {}
static void Headless_DestroyMaterial(Renderer* r, MaterialHandle handle) {}




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
    if (!r || !internal)
    {
        free(r);
        free(internal);
        return NULL;
    }
    memset(r, 0, sizeof(Renderer));
    memset(internal, 0, sizeof(Headless_Backend));
    
    internal->resource_counter = 1; // Start at 1, since 0 is usually "Invalid"
    internal->settings.gamma = 2.2f;
    internal->settings.exposure = 1.0f;
    internal->settings.max_draw_items = 32768;
    internal->settings.max_shadow_cascades = 4;
    internal->settings.max_reflection_probes = 16;
    
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
    r->CreateMaterial = Headless_CreateMaterial;
    r->UpdateMaterial = Headless_UpdateMaterial;
    r->DestroyMaterial = Headless_DestroyMaterial;

    r->CreateCubemap = Headless_CreateCubemap;
    r->CreateDynamicMesh = Headless_CreateDynamicMesh;
    r->UpdateDynamicMesh = Headless_UpdateDynamicMesh;

    r->DrawWorld = Headless_DrawWorld;

    r->SetSettings = Headless_SetSettings;
    r->GetSettings = Headless_GetSettings;

    r->UIinit = Headless_UIinit;
    r->UIShutdown = Headless_UIShutdown;
    r->UIRender = Headless_UIRender;
    r->DrawOverlay = Headless_DrawOverlay;

    return r;
}