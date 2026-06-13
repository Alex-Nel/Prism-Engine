#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include "../core/log.h"
#include "../core/mesh_core.h"
#include "render/render.h"


#define DEFAULT_SHADER  (ShaderHandle){0}
#define DEFAULT_TEXTURE (TextureHandle){0}



// A single part of a model
typedef struct ModelNode
{
    Mesh* mesh;
    Material* material;
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



// Loads assets from disk

Mesh* Asset_LoadMesh(const char* name, const char* filepath);
Texture* Asset_LoadTexture(const char* name, const char* filepath);
Shader* Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path);



// Functions to get built in meshes

Mesh* Asset_GetBuiltinQuad();
Mesh* Asset_GetBuiltinCube();
Mesh* Asset_GetBuiltinSphere();



// Functions to get a default texture and shader

Texture* Asset_GetDefaultTexture();
Shader* Asset_GetDefaultShader();



// Generate a texture from a solid color
Texture* Asset_CreateSolidColorTexture(const char* name, Color color);

// Material creation function
Material* Asset_CreateMaterial(Shader* shader, Texture* diffuse);



// Functions to retrieve meshes and textures by name

Model* Asset_GetModelByName(const char* name);
Mesh* Asset_GetMeshByName(const char* name);
Texture* Asset_GetTextureByName(const char* name);


// TODO: Make individual meshes of a model become separate entities




#endif