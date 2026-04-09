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
// 1. OPAQUE HANDLES Opaque Handles to meshes, textures, shaders, and materials
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





// Function pointer for loading graphics API procedures (Currently only OpenGL
typedef void* (*Render_LoadProcFn)(const char* name);


// Initializes a renderer with a specified graphics API
bool Render_Init(GraphicsAPI api, Render_LoadProcFn load_proc);

// Shuts down the renderer
void Render_Shutdown();



// Sets the size and position of the viewport
void Render_SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

// Sets the color of the renderer to clear with
void Render_SetClearColor(float r, float g, float b, float a);

// Clears the renderer
void Render_Clear(void);





// Uploads vertex and index data to the GPU and returns a handle
MeshHandle Render_CreateMesh(const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count);
// Removes a mesh from the GPU
void Render_DestroyMesh(MeshHandle mesh);


// Uploads pixels to the renderer to make a texture. Returns a handle
TextureHandle Render_CreateTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels);
// Removes a texture from the GPU
void Render_DestroyTexture(TextureHandle texture);


// Uploads vertex and fragment shaders to the GPU to make a complete shader. Returns a handle
ShaderHandle Render_CreateShader(const char* vertex_source, const char* fragment_source);
// Removes a shader from the GPU
void Render_DestroyShader(ShaderHandle shader);





// Sets the global camera matrices for the current frame
void Render_BeginFrame(const RenderPacket* packet);

// Adds an object to the draw queue
void Render_Submit(MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform);

// Sorts the queue, binds the state, and executes the actual GPU draw calls
void Render_EndFrame();



#endif