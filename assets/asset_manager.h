#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include "../core/log.h"
#include "render/render.h"


#define DEFAULT_SHADER  (ShaderHandle){0}
#define DEFAULT_TEXTURE (TextureHandle){0}

typedef struct Material
{
    uint32_t id;
    bool active;
    
    uint32_t shader_id;
    uint32_t diffuse_texture_id;

    MaterialProperties properties;
    
} Material;



// A single part of a model
typedef struct ModelNode
{
    MeshHandle mesh;
    MaterialHandle material;
} ModelNode;



// The container for an entire imported 3D file
typedef struct Model
{
    char name[64];
    
    ModelNode* nodes;    // Dynamic array of nodes (parts)
    uint32_t node_count; // How many parts this model has
} Model;



// The main loader function
Model* Asset_LoadModel(const char* name, const char* filepath);




// Initialize the registry arrays
void Asset_Init(Renderer* r);

// Loads mesh from disk
MeshHandle Asset_LoadMesh(const char* name, const char* filepath);

// Material creation function
MaterialHandle Asset_CreateMaterial(ShaderHandle shader, TextureHandle diffuse);
Material* Asset_GetMaterial(MaterialHandle handle);

// Loads shaders and textures and caches them
ShaderHandle Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path);
TextureHandle Asset_LoadTexture(const char* name, const char* filepath);

// A helper to quickly grab a standard Quad without copy-pasting code
MeshHandle Asset_GetBuiltinQuad();
MeshHandle Asset_GetBuiltinCube();
MeshHandle Asset_GetBuiltinSphere();

// Function to get a default texture and shader
TextureHandle Asset_GetDefaultTexture();
ShaderHandle Asset_GetDefaultShader();

// Get the meshdata from a cached mesh
MeshData* Asset_GetMeshData(MeshHandle handle);



#endif