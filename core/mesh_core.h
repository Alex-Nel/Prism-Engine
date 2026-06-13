#ifndef MESH_CORE_H
#define MESH_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "math_core.h"



typedef struct { uint32_t id; } MeshHandle;
typedef struct { uint32_t id; } TextureHandle;
typedef struct { uint32_t id; } ShaderHandle;



// Vertex3D struct for geometric data.
typedef struct Vertex3D
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
} Vertex3D;



// Color structure (in RGBA format)
typedef struct Color
{
    float r, g, b, a;
} Color;



// Struct for a global scene light
typedef struct DirectionalLight
{
    Vector3 direction;
    Color color;
    float ambient_strength;
} DirectionalLight;



// Struct for an Axis Aligned Bounding Box
typedef struct AABB
{
    Vector3 min;
    Vector3 max;
} AABB;



// Struct for all mesh data
typedef struct Mesh
{
    char name[64];

    // Mesh Data
    Vertex3D* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
    AABB local_bounds;

    // Index in asset manager
    uint32_t id;

    // GPU handle
    MeshHandle gpu_handle;
} Mesh;



// Structure for a texture
typedef struct Texture
{
    char name[64];
    uint32_t id;
    TextureHandle gpu_handle;
    int width, height, channels;
} Texture;



// Structure for a compiled shader
typedef struct Shader
{
    char name[64];
    uint32_t id;
    ShaderHandle gpu_handle;
} Shader;



// Properties for materials
typedef struct MaterialProperties
{
    Color tint_color;
    float shininess;
    float specular_strength;
} MaterialProperties;



// Structure for a material
typedef struct Material
{
    uint32_t id;
    bool active;
    
    Shader* shader;
    Texture* diffuse_texture;

    MaterialProperties properties;
    
} Material;





#endif