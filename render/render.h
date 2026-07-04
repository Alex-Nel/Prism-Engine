#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>

#include "../core/math_core.h"
#include "../core/mesh_core.h"
#include "../core/log.h"
#include "../core/io_core.h"





// Used for invalid handles
#define RENDER_INVALID_HANDLE 0



// --- Internal data pool limits ---

#define MAX_RESOURCES 1024
#define MAX_COMMANDS 4096

#define SHADOW_MAP_RESOLUTION 4096
#define MAX_SHADOW_CASCADES 4
#define SHADOW_CASCADE_COUNT_DEFAULT 1





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
    uint8_t shadow_cascade_count;   // 1 = single shadow map; 2-4 = cascaded shadow maps
    float shadow_max_distance;      // max shadow range from camera (CSM only)
    float cascade_split_lambda;     // 0 = uniform splits, 1 = logarithmic, 0.5 = practical
    float cascade_blend_fraction;   // 0..1 slice fraction cross-faded at each split (CSM only)
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
} SpotLightData;





// Struct for a render packet to send to renderer
typedef struct RenderPacket
{
    uint32_t window_width;
    uint32_t window_height;

    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;
    
    DirectionalLightData* dir_lights;
    uint32_t dir_light_count;
    
    PointLightData* point_lights; 
    uint32_t point_light_count;

    SpotLightData* spot_lights; 
    uint32_t spot_light_count;

    uint32_t shadow_cascade_count;
    Matrix4 light_space_matrices[MAX_SHADOW_CASCADES];
    float shadow_texel_world_sizes[MAX_SHADOW_CASCADES];
    float cascade_splits[MAX_SHADOW_CASCADES - 1]; // distance along camera forward
    Vector3 camera_forward; // main camera forward (cascade selection in shader)
    float shadow_camera_near;      // camera near plane (CSM blend region sizing)
    float cascade_blend_fraction;  // 0..1 fraction of each slice used to cross-fade

    bool enable_ssao;

    bool has_skybox;
    TextureHandle skybox_texture;
    ShaderHandle skybox_shader;
} RenderPacket;





// Unused - Texture filtering
typedef enum
{
    TEXTURE_FILTER_NEAREST, // Blocky (Pixel art style)
    TEXTURE_FILTER_LINEAR   // Smooth (Standard 3D style)
} TextureFilter;




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

    MeshHandle    (*CreateMesh)(Renderer* r, const Vertex3D* vertices, uint32_t v_count, const uint32_t* indices, uint32_t i_count);
    void          (*DestroyMesh)(Renderer* r, MeshHandle mesh);
    
    TextureHandle (*CreateTexture)(Renderer* r, const uint8_t* pixels, uint32_t w, uint32_t h, uint32_t channels);
    void          (*DestroyTexture)(Renderer* r, TextureHandle texture);

    ShaderHandle  (*CreateShader)(Renderer* r, const char* v_source, const char* f_source);
    void          (*DestroyShader)(Renderer* r, ShaderHandle shader);

    TextureHandle (*CreateCubemap)(Renderer* r, const uint8_t* right, const uint8_t* left, const uint8_t* top, const uint8_t* bottom, const uint8_t* front, const uint8_t* back, uint32_t width, uint32_t height, uint32_t channels);
    MeshHandle (*CreateDynamicMesh)(Renderer* r, uint32_t max_vertices, uint32_t max_indices);
    MeshHandle (*CreateSkinnedMesh)(Renderer* r, const Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count);
    void (*UpdateDynamicMesh)(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);



    // --- Command Submission ---
    void (*BeginShadowPass)(Renderer* r, const RenderPacket* packet);
    void (*EndShadowPass)(Renderer* r);
    void (*BeginFrame)(Renderer* r, const RenderPacket* packet);
    void (*Submit)(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat, Matrix4 transform, Matrix4* bone_matrices, bool is_transparent, float depth_distance);
    void (*EndFrame)(Renderer* r);

    // Hidden implementation-specific data (e.g., SDL_GLContext or Vulkan Instance)
    void* backend_internal_data;

} Renderer;






// Function pointer for loading graphics API procedures (Currently only OpenGL)
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





// Uploads vertex and index data to the GPU and returns a handle
static inline MeshHandle Render_CreateMesh(Renderer* r, const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count)
{
    if (r && r->CreateMesh)
        return r->CreateMesh(r, vertices, vertex_count, indices, index_count);\
    else
        return (MeshHandle){0};
}
// Removes a mesh from the GPU
static inline void Render_DestroyMesh(Renderer* r, MeshHandle mesh)
{
    if (r && r->DestroyMesh)
        r->DestroyMesh(r, mesh);
}


// Uploads pixels to the renderer to make a texture. Returns a handle
static inline TextureHandle Render_CreateTexture(Renderer* r, const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    if (r && r->CreateTexture)
        return r->CreateTexture(r, pixels, width, height, channels);
    else
        return (TextureHandle){0};
}
// Removes a texture from the GPU
static inline void Render_DestroyTexture(Renderer* r, TextureHandle texture)
{
    if (r && r->DestroyTexture)
        r->DestroyTexture(r, texture);
}


// Uploads vertex and fragment shaders to the GPU to make a complete shader. Returns a handle
static inline ShaderHandle Render_CreateShader(Renderer* r, const char* vertex_source, const char* fragment_source)
{
    if (r && r->CreateShader)
        return r->CreateShader(r, vertex_source, fragment_source);
    else
        return (ShaderHandle){0};
}
// Removes a shader from the GPU
static inline void Render_DestroyShader(Renderer* r, ShaderHandle shader)
{
    if (r && r->DestroyShader)
        r->DestroyShader(r, shader);
}


// Creates a CubeMap for the skybox. Returns a texture handle
static inline TextureHandle Render_CreateCubemap(Renderer* r, 
    const uint8_t* right, const uint8_t* left, const uint8_t* top, 
    const uint8_t* bottom, const uint8_t* front, const uint8_t* back, 
    uint32_t width, uint32_t height, uint32_t channels)
{
    if (r && r->CreateCubemap)
        return r->CreateCubemap(r, right, left, top, bottom, front, back, width, height, channels);
    else
        return (TextureHandle){0};
}


// Creates a skinned mesh. Returns a mesh handle 
static inline MeshHandle Render_CreateSkinnedMesh(Renderer* r, const Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    if (r && r->CreateSkinnedMesh)
        return r->CreateSkinnedMesh(r, vertices, vertex_count, indices, index_count);
    return (MeshHandle){0};
}


// Creates a dynamic mesh. Returns a mesh handle
static inline MeshHandle Render_CreateDynamicMesh(Renderer* r, uint32_t max_vertices, uint32_t max_indices)
{
    if (r && r->CreateDynamicMesh)
        return r->CreateDynamicMesh(r, max_vertices, max_indices);
    else
        return (MeshHandle){0};
}


// Quickly overwrites the existing GPU memory with new vertex data
static inline void Render_UpdateDynamicMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
{
    if (r && r->UpdateDynamicMesh)
        r->UpdateDynamicMesh(r, handle, vertices, vertex_count, indices, index_count);   
}





// Starts the shadow pass of the render pipeline
static inline void Render_BeginShadowPass(Renderer* r, const RenderPacket* packet)
{
    if (r && r->BeginShadowPass)
        r->BeginShadowPass(r, packet);
}

// Ends the shadow pass of the render pipeline
static inline void Render_EndShadowPass(Renderer* r)
{
    if (r && r->EndShadowPass)
        r->EndShadowPass(r);
}

// Sets the global camera matrices for the current frame
static inline void Render_BeginFrame(Renderer* r, const RenderPacket* packet)
{
    if (r && r->BeginFrame)
        r->BeginFrame(r, packet);
}

// Adds an object to the draw queue
static inline void Render_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform, Matrix4* bone_matrices, bool is_transparent, float depth_distance)
{
    if (r && r->Submit)
        r->Submit(r, mesh, shader, texture, mat_props, transform, bone_matrices, is_transparent, depth_distance);
}

// Sorts the queue, binds the state, and executes the actual GPU draw calls
static inline void Render_EndFrame(Renderer* r)
{
    if (r && r->EndFrame)
        r->EndFrame(r);
}



#endif