#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>

#include "../core/math_core.h"
#include "../core/mesh_core.h"
#include "../core/log_core.h"
#include "../core/io_core.h"
#include "../core/overlay_core.h"
#include "../core/frustum_core.h"





// Used for invalid handles
#define RENDER_INVALID_HANDLE 0





// Enum for different graphics API's
typedef enum GraphicsAPI
{
    GRAPHICS_API_OPENGL,
    GRAPHICS_API_VULKAN,
    GRAPHICS_API_DIRECTX,
    GRAPHICS_API_SOFTWARE,
    GRAPHICS_API_NONE
} GraphicsAPI;





// Struct for directional light data
typedef struct DirectionalLightData
{
    Vector3 direction;
    Color color;
    float intensity;
    float ambient_strength;
    float shadow_box_size;
    uint8_t shadow_cascade_count;   // 1 = single shadow map; more = cascaded shadow maps; Clamped by settings
    float shadow_max_distance;      // max shadow range from camera (CSM only)
    float cascade_split_lambda;     // 0 = uniform splits, 1 = logarithmic, 0.5 = practical
    float cascade_blend_fraction;   // 0..1 slice fraction cross-faded at each split (CSM only)
    bool casts_shadows;
} DirectionalLightData;





// Struct for point light data
typedef struct PointLightData
{
    Vector3 position;
    Color color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    bool casts_shadows;
} PointLightData;





// Struct for spot light data
typedef struct SpotLightData
{
    Vector3 position;
    Vector3 direction;
    Color color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float inner_cut_off;
    float outer_cut_off;
    bool casts_shadows;
} SpotLightData;





// Struct for reflection probe data
typedef struct ReflectionProbeData
{
    uint32_t entity_id;
    Vector3 position;
    Vector3 box_extents;
    float blend_distance;
    int32_t priority;
    uint32_t capture_resolution;
    uint32_t revision;
    bool needs_capture;
    EnvironmentMapHandle environment;
    bool dirty;
    bool captured;
} ReflectionProbeData;





// What the backend should wipe before drawing this view
typedef enum RenderClearFlags
{
    RENDER_CLEAR_COLOR_AND_DEPTH = 0,
    RENDER_CLEAR_DEPTH_ONLY = 1,
    RENDER_CLEAR_NONE = 2
} RenderClearFlags;





// Flags for a submitted render item
enum
{
    RENDER_ITEM_TRANSPARENT     = 1u << 0,
    RENDER_ITEM_CAST_SHADOWS    = 1u << 1,
    RENDER_ITEM_RECEIVE_SHADOWS = 1u << 2,
    RENDER_ITEM_PROBE_CAPTURE   = 1u << 3
};





// CPU-side material description uploaded to the backend
typedef struct RenderMaterialDesc
{
    ShaderHandle shader;          // 0 = default PBR path
    TextureHandle albedo;
    TextureHandle normal;
    TextureHandle metallic;
    TextureHandle roughness;
    TextureHandle ao;
    MaterialProperties properties;
} RenderMaterialDesc;










// ----- Structs for resources -----

// Enum for the format of a vertex shader
typedef enum RenderVertexFormat
{
    RENDER_VERTEX_STATIC = 0,
    RENDER_VERTEX_SKINNED = 1
} RenderVertexFormat;





// Enum for the usage of a mesh
typedef enum RenderMeshUsage
{
    RENDER_MESH_STATIC = 0,
    RENDER_MESH_DYNAMIC = 1
} RenderMeshUsage;





// Struct for the description of a mesh
typedef struct RenderMeshDesc
{
    RenderVertexFormat vertex_format;
    RenderMeshUsage usage;
    const void* vertices;         // Vertex3D* or Vertex3DSkinned*; NULL for empty dynamic meshes
    uint32_t vertex_count;
    const uint32_t* indices;
    uint32_t index_count;
    uint32_t max_vertices;        // dynamic reserve; 0 = vertex_count
    uint32_t max_indices;
} RenderMeshDesc;





// Structure used to update a mesh
typedef struct RenderMeshUpdate
{
    const void* vertices;
    uint32_t vertex_count;
    const uint32_t* indices;
    uint32_t index_count;
} RenderMeshUpdate;










// Enum for a type of texture
typedef enum RenderTextureType
{
    RENDER_TEXTURE_2D = 0,
    RENDER_TEXTURE_CUBE = 1
} RenderTextureType;





// Enum for pixel formats
typedef enum RenderPixelFormat
{
    RENDER_FORMAT_R8 = 0,
    RENDER_FORMAT_RG8,
    RENDER_FORMAT_RGB8,
    RENDER_FORMAT_RGBA8,
    RENDER_FORMAT_RGB16F,
    RENDER_FORMAT_RGBA16F
} RenderPixelFormat;





// Enu for the texture filtering
typedef enum RenderTextureFilter
{
    RENDER_FILTER_DEFAULT = 0,    // backend chooses (OpenGL: 1x1 nearest, else linear mips)
    RENDER_FILTER_NEAREST,
    RENDER_FILTER_LINEAR
} RenderTextureFilter;





// Struct for the description of a texture
typedef struct RenderTextureDesc
{
    RenderTextureType type;
    RenderPixelFormat format;
    uint32_t width;
    uint32_t height;
    RenderTextureFilter min_filter;
    RenderTextureFilter mag_filter;
    const void* pixels;           // 2D texel data (uint8 or float)
    const void* cube_faces[6];    // right, left, top, bottom, front, back
} RenderTextureDesc;










// Enum for the format of a shader
typedef enum RenderShaderFormat
{
    RENDER_SHADER_GLSL_SOURCE = 0,
    RENDER_SHADER_SPIRV = 1
} RenderShaderFormat;





// Struct for the description of a shader
typedef struct RenderShaderDesc
{
    RenderShaderFormat format;
    const void* vertex_code;
    uint32_t vertex_size;         // 0 = NUL-terminated string
    const void* fragment_code;
    uint32_t fragment_size;
} RenderShaderDesc;





// Struct for the description of an environment map
typedef struct RenderEnvironmentMapDesc
{
    const float* hdr_pixels;   // If set, bake skybox + IBL
    uint32_t width;
    uint32_t height;
    TextureHandle skybox;      // If hdr_pixels is NULL, wrap this cubemap as skybox only
} RenderEnvironmentMapDesc;











// ----- Structs to send to a renderer -----

// One drawable submitted to the renderer for the current view
typedef struct RenderItem
{
    MeshHandle mesh;
    MaterialHandle material;
    Matrix4 transform;
    AABB local_bounds;            // mesh-space AABB
    Matrix4* bone_matrices;       // NULL if static, gets copied to the backend
    Color color;
    float depth_distance;         // transparent sort
    uint32_t flags;
} RenderItem;





// Struct for a cameras viewport, matrices, and clear
typedef struct RenderView
{
    uint32_t window_width;
    uint32_t window_height;

    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;
    
    RenderClearFlags clear_flags;
    Color clear_color;

    bool has_env_map;    // Whether this view draws the environment (skybox / IBL). Overlay cameras often turn this off.
} RenderView;





// Struct for a render packet to send to renderer
typedef struct RenderLighting
{
    Vector3 shadow_camera_pos;
    Vector3 camera_forward;
    Vector3 camera_right;
    Vector3 camera_up;
    float camera_near;
    float camera_far;
    float camera_fov;
    float camera_aspect;
    
    DirectionalLightData* dir_lights;
    uint32_t dir_light_count;
    
    PointLightData* point_lights; 
    uint32_t point_light_count;

    SpotLightData* spot_lights; 
    uint32_t spot_light_count;

    ReflectionProbeData* reflection_probes;
    uint32_t reflection_probe_count;

    bool enable_ssao;
    Color global_ambient_color;
    float global_ambient_illumination;
    float gamma;
    float exposure;

    EnvironmentMapHandle env_map;
    bool has_probe_source_env_map;
    EnvironmentMapHandle probe_source_env_map;
} RenderLighting;





// View snapshot. Caller pointers only need to stay valid for the DrawWorld call. The backend copies items, bones, lights, and probes into its own storage.
typedef struct RenderWorld
{
    RenderView view;
    RenderLighting lighting;
    const RenderItem* items;
    uint32_t item_count;
} RenderWorld;










// Structure holding all renderer settings. Acts as the policy that the renderer uses
typedef struct RendererSettings
{
    bool enable_ssao;
    uint32_t shadow_map_resolution; // e.g., 1024, 2048, 4096
    float gamma;                    // e.g., 2.2f (default)
    float exposure;                 // e.g., 1.0f (default)

    uint32_t max_draw_items;
    uint32_t max_shadow_cascades;
    uint32_t max_reflection_probes;
} RendererSettings;





typedef struct Renderer Renderer;

typedef struct Renderer
{
    GraphicsAPI api;

    // --- Lifecycle ---
    
    void (*Shutdown)(Renderer* r);



    // --- State ---

    void (*SetViewport)(Renderer* r, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void (*SetClearColor)(Renderer* renderer, float r, float g, float b, float a);
    void (*Clear)(Renderer* r);
    void (*ClearDepth)(Renderer* r);



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



    // --- UI Rendering ---

    void (*UIinit)(Renderer* r, void* nk_ctx);
    void (*UIShutdown)(Renderer* r);
    void (*UIRender)(Renderer* r, void* nk_ctx, uint32_t width, uint32_t height);
    void (*DrawOverlay)(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height);



    // --- Settings ---

    void (*SetSettings)(Renderer* r, const RendererSettings* settings);
    RendererSettings (*GetSettings)(Renderer* r);



    // --- Hidden implementation-specific data ---
    void* backend_internal_data;

} Renderer;






// Function pointer for loading graphics API procedures
typedef void* (*Render_LoadProcFn)(const char* name);


// Initializes a renderer with a specified graphics API
Renderer* Render_Init(GraphicsAPI api, Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height);

// Shuts down the renderer
static inline void Render_Shutdown(Renderer* r)
{
    if (r && r->Shutdown)
        r->Shutdown(r);
}



// Sets the size and position of the viewport
static inline void Render_SetViewport(Renderer* r, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (r && r->SetViewport)
        r->SetViewport(r, x, y, width, height);
}

// Sets the color of the renderer to clear with
static inline void Render_SetClearColor(Renderer* r, float red, float green, float blue, float alpha)
{
    if (r && r->SetClearColor)
        r->SetClearColor(r, red, green, blue, alpha);
}

// Clears the renderer
static inline void Render_Clear(Renderer* r)
{
    if (r && r->Clear)
        r->Clear(r);
}

// Clears the depth buffer
static inline void Render_ClearDepth(Renderer* r)
{
    if (r && r->ClearDepth)
        r->ClearDepth(r);
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
    if (r && r->CreateMesh && desc)
        return r->CreateMesh(r, desc);
    return (MeshHandle){0};
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
    if (r && r->UpdateMesh && update)
        r->UpdateMesh(r, handle, update);
}
// Removes a mesh from the GPU
static inline void Render_DestroyMesh(Renderer* r, MeshHandle mesh)
{
    if (r && r->DestroyMesh)
        r->DestroyMesh(r, mesh);
}



// Uploads pixels to the renderer to make a texture. Returns a handle
static inline TextureHandle Render_CreateTexture(Renderer* r, const RenderTextureDesc* desc)
{
    if (r && r->CreateTexture && desc)
        return r->CreateTexture(r, desc);
    return (TextureHandle){0};
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
    if (r && r->DestroyTexture)
        r->DestroyTexture(r, texture);
}



// Uploads a shader program. Returns a handle
static inline ShaderHandle Render_CreateShader(Renderer* r, const RenderShaderDesc* desc)
{
    if (r && r->CreateShader && desc)
        return r->CreateShader(r, desc);
    return (ShaderHandle){0};
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
    if (r && r->DestroyShader)
        r->DestroyShader(r, shader);
}



// Creates a GPU material from a CPU description
static inline MaterialHandle Render_CreateMaterial(Renderer* r, const RenderMaterialDesc* desc)
{
    if (r && r->CreateMaterial && desc)
        return r->CreateMaterial(r, desc);
    else
        return (MaterialHandle){0};
}
// Updates an existing GPU material
static inline void Render_UpdateMaterial(Renderer* r, MaterialHandle handle, const RenderMaterialDesc* desc)
{
    if (r && r->UpdateMaterial && desc)
        r->UpdateMaterial(r, handle, desc);
}
// Removes a GPU material
static inline void Render_DestroyMaterial(Renderer* r, MaterialHandle handle)
{
    if (r && r->DestroyMaterial)
        r->DestroyMaterial(r, handle);
}



// Creates GPU IBL resources (or wraps a cubemap as a skybox-only environment). Returns a handle
static inline EnvironmentMapHandle Render_CreateEnvironmentMap(Renderer* r, const RenderEnvironmentMapDesc* desc)
{
    if (r && r->CreateEnvironmentMap && desc)
        return r->CreateEnvironmentMap(r, desc);
    return (EnvironmentMapHandle){0};
}
static inline void Render_DestroyEnvironmentMap(Renderer* r, EnvironmentMapHandle handle)
{
    if (r && r->DestroyEnvironmentMap)
        r->DestroyEnvironmentMap(r, handle);
}










// Draws a complete view snapshot. Backends must implement DrawWorld.
static inline void Render_DrawWorld(Renderer* r, const RenderWorld* world)
{
    if (r->DrawWorld && world)
        r->DrawWorld(r, world);
}










// Initializes the UI rendering pipeline
static inline void Render_UIinit(Renderer* r, void* nk_ctx)
{
    if (r && r->UIinit)
        r->UIinit(r, nk_ctx);
}

// Shuts down the UI rendering pipeline
static inline void Render_UIShutdown(Renderer* r)
{
    if (r && r->UIShutdown)
        r->UIShutdown(r);
}

// Renders any UI
static inline void Render_UIRender(Renderer* r, void* nk_ctx, uint32_t width, uint32_t height)
{
    if (r && r->UIRender)
        r->UIRender(r, nk_ctx, width, height);
}

// Renders any Overlay
static inline void Render_DrawOverlay(Renderer* r, const OverlayDrawList* list, uint32_t width, uint32_t height)
{
    if (r && r->DrawOverlay)
        r->DrawOverlay(r, list, width, height);
}










// Sets all renderer settings according to the specified struct
static inline void Render_SetSettings(Renderer* r, const RendererSettings* settings)
{
    if (r && r->SetSettings && settings)
        r->SetSettings(r, settings);
}

// Returns all the settings of the renderer
static inline RendererSettings Render_GetSettings(Renderer* r)
{
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