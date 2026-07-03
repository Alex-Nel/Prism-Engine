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
    bool is_skinned;
    Mesh* mesh;
    SkinnedMesh* skinned_mesh;
    Material* material;
} ModelNode;



// The container for an entire imported 3D file
typedef struct Model
{
    char name[MAX_NAME_LENGTH];
    
    ModelNode* nodes;    // Dynamic array of nodes (parts)
    uint32_t node_count; // How many parts this model has

    Skeleton* skeleton;

    uint32_t animation_count;
    AnimationClip** animations;
} Model;



// The main loader function
Model* Asset_LoadModel(const char* name, const char* filepath);




// Initialize the registry arrays
void Asset_Init(Renderer* r);



// Loads assets from disk

Mesh* Asset_LoadMesh(const char* name, const char* filepath);
Texture* Asset_LoadTexture(const char* name, const char* filepath);
Texture* Asset_LoadSkyboxTexture(const char* name, const char* right, const char* left, const char* top, const char* bottom, const char* front, const char* back);
Shader* Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path);



// Functions to get built in meshes

Mesh* Asset_GetBuiltinQuad();
Mesh* Asset_GetBuiltinCube();
Mesh* Asset_GetBuiltinSphere();



// Returns the default texture
Texture* Asset_GetDefaultTexture();



// Generate a texture from a solid color
Texture* Asset_CreateSolidColorTexture(const char* name, Color color);

// Material creation function
Material* Asset_CreateMaterial(Shader* shader, Texture* diffuse);

// Creates a dynamic mesh from the renderer
Mesh* Asset_CreateDynamicMesh(uint32_t max_vertices, uint32_t max_indices);

// Updates a dynamic mesh from the renderer
void Asset_UpdateDynamicMesh(Mesh* mesh, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);



// Functions to retrieve meshes and textures by name

Model* Asset_GetModelByName(const char* name);
Mesh* Asset_GetMeshByName(const char* name);
SkinnedMesh* Asset_GetSkinnedMeshByName(const char* name);
Texture* Asset_GetTextureByName(const char* name);


// TODO: Make individual meshes of a model become separate entities




#endif