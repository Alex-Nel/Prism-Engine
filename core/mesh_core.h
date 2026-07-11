#ifndef MESH_CORE_H
#define MESH_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "math_core.h"

#define MAX_NAME_LENGTH 256
#define MAX_BONES 128
#define MAX_BONE_INFLUENCE 4



typedef struct { uint32_t id; } MeshHandle;
typedef struct { uint32_t id; } TextureHandle;
typedef struct { uint32_t id; } ShaderHandle;



// Vertex3D struct for geometric data.
typedef struct Vertex3D
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Vector3 tangent;
} Vertex3D;



// Vertex3D struct for skinned mesh data.
typedef struct Vertex3DSkinned
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    Vector3 tangent;

    int bone_ids[MAX_BONE_INFLUENCE];
    float bone_weights[MAX_BONE_INFLUENCE];
} Vertex3DSkinned;



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



// Struct for static mesh data
typedef struct Mesh
{
    char name[MAX_NAME_LENGTH];

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



// Struct for skinned (dynamic) mesh data
typedef struct SkinnedMesh
{
    char name[MAX_NAME_LENGTH];

    // Mesh Data
    Vertex3DSkinned* vertices;
    uint32_t vertex_count;
    uint32_t* indices;
    uint32_t index_count;
    AABB local_bounds;

    // Index in asset manager
    uint32_t id;

    // GPU handle
    MeshHandle gpu_handle;
} SkinnedMesh;



// Structure for a texture
typedef struct Texture
{
    char name[MAX_NAME_LENGTH];
    uint32_t id;
    TextureHandle gpu_handle;
    int width, height, channels;
} Texture;



// Structure for a compiled shader
typedef struct Shader
{
    char name[MAX_NAME_LENGTH];
    uint32_t id;
    ShaderHandle gpu_handle;
} Shader;



// Properties for materials
typedef struct MaterialProperties
{
    Color albedo_tint;
    float metallic_factor;    // 0.0 = plastic, 1.0 metal
    float roughness_factor;   // 0.0 = perfect mirror, 1.0 = rough dirt
} MaterialProperties;



// Structure for a material
typedef struct Material
{
    uint32_t id;
    bool active;
    
    Shader* shader;

    Texture* albedo_texture;
    Texture* normal_map;
    Texture* metallic_map;   // White = metal, Black = non-metal
    Texture* roughness_map;  // White = rough, Black = smooth
    Texture* ao_map;         // White = unoccluded (1.0), Black = occluded (0.0)

    MaterialProperties properties;
} Material;





// A single keyframe on the timeline
typedef struct VectorKey
{
    float time;
    Vector3 value;
} VectorKey;

typedef struct QuaternionKey
{
    float time;
    Quaternion value;
} QuaternionKey;



// Represents the timeline for ONE specific bone
typedef struct AnimationChannel
{
    char bone_name[MAX_NAME_LENGTH];
    
    VectorKey* positions;
    uint32_t position_count;
    
    QuaternionKey* rotations;
    uint32_t rotation_count;
    
    VectorKey* scales;
    uint32_t scale_count;
} AnimationChannel;



// Represents a full animation
typedef struct AnimationClip
{
    char name[MAX_NAME_LENGTH];
    float duration_ticks;
    float ticks_per_second;
    
    AnimationChannel* channels;
    uint32_t channel_count;
} AnimationClip;





// Represents a single bone in the skeleton
typedef struct BoneInfo
{
    char name[MAX_NAME_LENGTH];
    Matrix4 inverse_bind; // The matrix that transforms a vertex from Mesh Space to Bone Space
} BoneInfo;



// One node of a complete skeleton
typedef struct SkeletonNode
{
    char name[MAX_NAME_LENGTH];
    Matrix4 default_local_transform;
    
    struct SkeletonNode* children;
    uint32_t child_count;
} SkeletonNode;



// The Skeleton Asset (Shared by all meshes in a model)
typedef struct Skeleton
{
    BoneInfo bones[MAX_BONES];
    uint32_t bone_count;

    SkeletonNode* root_node;
} Skeleton;





// Builds a model-space AABB from the current skinned pose
AABB SkinnedMesh_ComputePoseAABB(const SkinnedMesh* mesh, const Matrix4* bone_matrices);

// Calculates Tangest Vectors for a mesh based on its UV coordinates
void Mesh_CalculateVertexTangents(Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count);
void Mesh_CalculateVertexSkinnedTangents(Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count);





#endif