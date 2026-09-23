#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H



#include "../core/mesh_core.h"
#include "../core/font_core.h"
#include "render/render.h"



#define MAX_CACHED_SHADERS 8192
#define MAX_CACHED_TEXTURES 8192
#define MAX_CACHED_MESHES 8192
#define MAX_CACHED_SKINNED_MESHES 8192
#define MAX_CACHED_MODELS 8192
#define MAX_CACHED_FONTS 256
#define MAX_MATERIALS 8192



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





// Structure for an asset manager
typedef struct AssetManager
{
    // --- Pointer to renderer ---

    Renderer* renderer;



    // --- Asset caches ---
    Shader shader_cache[MAX_CACHED_SHADERS];
    uint32_t shader_count;

    Texture texture_cache[MAX_CACHED_TEXTURES];
    uint32_t texture_count;

    Mesh mesh_cache[MAX_CACHED_MESHES];
    uint32_t mesh_count;

    SkinnedMesh skinned_mesh_cache[MAX_CACHED_SKINNED_MESHES];
    uint32_t skinned_mesh_count;

    Model* model_cache[MAX_CACHED_MODELS];
    uint32_t model_count;

    Font font_cache[MAX_CACHED_FONTS];
    uint32_t font_count;

    Material material_pool[MAX_MATERIALS];
    uint32_t material_count;

    RenderMaterialDesc material_gpu_desc[MAX_MATERIALS];
    bool material_gpu_desc_valid[MAX_MATERIALS];

    EnvironmentMap env_map_cache[MAX_CACHED_TEXTURES];
    uint32_t env_map_count;



    // --- Default assets ---

    Mesh* builtin_quad;
    Mesh* builtin_cube;
    Mesh* builtin_sphere;
    Texture* default_texture;
} AssetManager;










// Initialize the registry arrays
void Asset_Init(Renderer* r);



// Loads assets from disk

Model* Asset_LoadModel(const char* name, const char* filepath);
Mesh* Asset_LoadMesh(const char* name, const char* filepath);
Texture* Asset_LoadTexture(const char* name, const char* filepath);
Texture* Asset_LoadCubemapTexture(const char* name, const char* right, const char* left, const char* top, const char* bottom, const char* front, const char* back);
EnvironmentMap* Asset_LoadEnvironmentMap(const char* filepath);
EnvironmentMap* Asset_LoadEnvironmentMapFromSkybox(const char* name, const char* right, const char* left, const char* top, const char* bottom, const char* front, const char* back);
Font* Asset_LoadFont(const char* name, const char* filepath, float pixel_height);
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

// Uploads or refreshes the GPU copy of a CPU material
void Asset_SyncMaterialGPU(Material* material);

// Creates a dynamic mesh from the renderer
Mesh* Asset_CreateDynamicMesh(uint32_t max_vertices, uint32_t max_indices);

// Updates a dynamic mesh from the renderer
void Asset_UpdateDynamicMesh(Mesh* mesh, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);

// Updates a static mesh
void Asset_UpdateMesh(Mesh* mesh, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);



// Functions to retrieve assets by name

Model* Asset_GetModelByName(const char* name);
Mesh* Asset_GetMeshByName(const char* name);
SkinnedMesh* Asset_GetSkinnedMeshByName(const char* name);
Texture* Asset_GetTextureByName(const char* name);
Font* Asset_GetFontByName(const char* name);






#endif