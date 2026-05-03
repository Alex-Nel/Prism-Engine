#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>

#include "../core/mathCore.h"
#include "../core/meshCore.h"
#include "../core/log.h"




// Enum for different graphics API's
typedef enum GraphicsAPI
{
    GRAPHICS_API_OPENGL,
    GRAPHICS_API_VULKAN,
    GRAPHICS_API_DIRECTX,
    GRAPHICS_API_NONE     // Could be useful for headless dedicated servers
} GraphicsAPI;




// Struct for point light data
typedef struct PointLightData
{
    Vector3 position;
    Vector3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
} PointLightData;





// Struct for all meterial properties
typedef struct MaterialProperties
{
    Vector3 tint_color;
    float shininess;
    float specular_strength;
} MaterialProperties;




// Struct for a render packet to send to renderer
typedef struct RenderPacket
{
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;
    
    DirectionalLight global_light;
    PointLightData* point_lights; 
    uint32_t point_light_count;
} RenderPacket;




// -----------------------------------------------------------------------------
// Opaque Handles to meshes, textures, shaders, and materials
// -----------------------------------------------------------------------------

typedef struct { uint32_t id; } MeshHandle;
typedef struct { uint32_t id; } TextureHandle;
typedef struct { uint32_t id; } ShaderHandle;
typedef struct { uint32_t id; } MaterialHandle;

// Used for invalid handles
#define RENDER_INVALID_HANDLE 0



// --- Internal data pool limits ---
#define MAX_RESOURCES 1024
#define MAX_COMMANDS 4096




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

    // Lifecycle
    void (*Shutdown)(Renderer* r);

    // State
    void (*SetViewport)(Renderer* r, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void (*SetClearColor)(Renderer* renderer, float r, float g, float b, float a);
    void (*Clear)(Renderer* r);


    // Resource Management
    MeshHandle    (*CreateMesh)(Renderer* r, const Vertex3D* vertices, uint32_t v_count, const uint32_t* indices, uint32_t i_count);
    void          (*DestroyMesh)(Renderer* r, MeshHandle mesh);
    
    TextureHandle (*CreateTexture)(Renderer* r, const uint8_t* pixels, uint32_t w, uint32_t h, uint32_t channels);
    void          (*DestroyTexture)(Renderer* r, TextureHandle texture);

    ShaderHandle  (*CreateShader)(Renderer* r, const char* v_source, const char* f_source);
    void          (*DestroyShader)(Renderer* r, ShaderHandle shader);

    // Command Submission
    void (*BeginFrame)(Renderer* r, const RenderPacket* packet);
    void (*Submit)(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat, Matrix4 transform);
    void (*EndFrame)(Renderer* r);

    // Hidden implementation-specific data (e.g., SDL_GLContext or Vulkan Instance)
    void* backend_internal_data;

} Renderer;






// Function pointer for loading graphics API procedures (Currently only OpenGL
typedef void* (*Render_LoadProcFn)(const char* name);


// Initializes a renderer with a specified graphics API
Renderer* Render_Init(Render_LoadProcFn load_proc);

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





// Sets the global camera matrices for the current frame
static inline void Render_BeginFrame(Renderer* r, const RenderPacket* packet)
{
    if (r && r->BeginFrame)
        r->BeginFrame(r, packet);
}

// Adds an object to the draw queue
static inline void Render_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform)
{
    if (r && r->Submit)
        r->Submit(r, mesh, shader, texture, mat_props, transform);
}

// Sorts the queue, binds the state, and executes the actual GPU draw calls
static inline void Render_EndFrame(Renderer* r)
{
    if (r && r->EndFrame)
        r->EndFrame(r);
}



#endif