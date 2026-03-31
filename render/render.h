#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <stdbool.h>

#include "../core/mathCore.h"
#include "../core/meshCore.h"




// Enum for different graphics API's
typedef enum GraphicsAPI
{
    GRAPHICS_API_OPENGL,
    GRAPHICS_API_VULKAN,
    GRAPHICS_API_DIRECTX,
    GRAPHICS_API_NONE     // Useful for headless dedicated servers
} GraphicsAPI;




// Structure for point light data
typedef struct PointLightData
{
    Vector3 position;
    Vector3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
} PointLightData;





typedef struct MaterialProperties
{
    Vector3 tint_color;
    float shininess;
    float specular_strength;
} MaterialProperties;




// Structure for a render packet to send to renderer
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
// 1. OPAQUE HANDLES
// By wrapping the IDs in structs, the C compiler provides strict type-checking.
// You cannot accidentally pass a TextureHandle into a function expecting a MeshHandle.
// -----------------------------------------------------------------------------
typedef struct { uint32_t id; } MeshHandle;
typedef struct { uint32_t id; } TextureHandle;
typedef struct { uint32_t id; } ShaderHandle;
typedef struct { uint32_t id; } MaterialHandle;


#define RENDER_INVALID_HANDLE 0



// --- INTERNAL DATA POOLS ---
#define MAX_RESOURCES 1024
#define MAX_COMMANDS 4096




// -----------------------------------------------------------------------------
// 2. DATA STRUCTURES
// -----------------------------------------------------------------------------

typedef enum
{
    TEXTURE_FILTER_NEAREST, // Blocky (Pixel art style)
    TEXTURE_FILTER_LINEAR   // Smooth (Standard 3D style)
} TextureFilter;







// -----------------------------------------------------------------------------
// 3. SYSTEM MANAGEMENT
// -----------------------------------------------------------------------------

// Function pointer for loading graphics API procedures (e.g., SDL_GL_GetProcAddress)
typedef void* (*Render_LoadProcFn)(const char* name);

bool Render_Init(GraphicsAPI api, Render_LoadProcFn load_proc);
void Render_Shutdown(void);







// -----------------------------------------------------------------------------
// 4. GLOBAL STATE
// -----------------------------------------------------------------------------

void Render_SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void Render_SetClearColor(float r, float g, float b, float a);
void Render_Clear(void);







// -----------------------------------------------------------------------------
// 5. RESOURCE MANAGEMENT (GPU Memory)
// -----------------------------------------------------------------------------

// Uploads vertex and index data to the GPU and returns a handle
MeshHandle Render_CreateMesh(const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count);
void Render_DestroyMesh(MeshHandle mesh);


// TextureHandle Render_CreateTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels, TextureFilter filter);
TextureHandle Render_CreateTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels);
void Render_DestroyTexture(TextureHandle texture);


ShaderHandle Render_CreateShader(const char* vertex_source, const char* fragment_source);
void Render_DestroyShader(ShaderHandle shader);







// -----------------------------------------------------------------------------
// 6. THE SUBMISSION QUEUE
// -----------------------------------------------------------------------------

// Sets the global camera matrices for the current frame
void Render_BeginFrame(const RenderPacket* packet);

// Adds an object to the draw queue
void Render_Submit(MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform);

// Sorts the queue, binds the state, and executes the actual GPU draw calls
void Render_EndFrame(void);

#endif