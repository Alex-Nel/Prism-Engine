#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"
#include "asset_manager.h"
#include "core/io_core.h"
#include "core/mesh_core.h"
#include "core/image.h"



#define MAX_CACHED_SHADERS 1024
#define MAX_CACHED_TEXTURES 1024
#define MAX_CACHED_MESHES 1024
#define MAX_CACHED_SKINNED_MESHES 1024
#define MAX_CACHED_MODELS 1024
#define MAX_MATERIALS 1024



static Renderer* renderer = NULL;



static Shader shader_cache[MAX_CACHED_SHADERS];
static uint32_t shader_count = 0;

static Texture texture_cache[MAX_CACHED_TEXTURES];
static uint32_t texture_count = 0;

static Mesh mesh_cache[MAX_CACHED_MESHES];
static uint32_t mesh_count = 0;

static SkinnedMesh skinned_mesh_cache[MAX_CACHED_SKINNED_MESHES];
static uint32_t skinned_mesh_count = 0;

static Model* model_cache[MAX_CACHED_MODELS];
static uint32_t model_count = 0;

static Material material_pool[MAX_MATERIALS];
static uint32_t material_count = 0;





static Mesh* builtin_quad = NULL;
static Mesh* builtin_cube = NULL;
static Mesh* builtin_sphere = NULL;
static Texture* default_texture = NULL;
static Shader* default_shader = NULL;
static Shader* default_animated_shader = NULL;
static Shader* default_skybox_shader = NULL;
static Shader* default_shadow_shader = NULL;
static Shader* default_skinned_shadow_shader = NULL;





void Asset_Init(Renderer* r)
{
    renderer = r;
    
    shader_count = 0;
    texture_count = 0;
    mesh_count = 0;
    material_count = 0;
}





// Calculates the AABB box for a specified mesh (set of vertices)
static AABB CalculateAABB_Static(const Vertex3D* vertices, uint32_t vertex_count)
{
    // Start min at the highest possible number, and max at the lowest possible number
    AABB bounds = {
        (Vector3){ FLT_MAX,  FLT_MAX,  FLT_MAX},
        (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX}
    };

    for (uint32_t i = 0; i < vertex_count; i++)
    {
        Vector3 p = vertices[i].position;

        if (p.x < bounds.min.x) bounds.min.x = p.x;
        if (p.y < bounds.min.y) bounds.min.y = p.y;
        if (p.z < bounds.min.z) bounds.min.z = p.z;

        if (p.x > bounds.max.x) bounds.max.x = p.x;
        if (p.y > bounds.max.y) bounds.max.y = p.y;
        if (p.z > bounds.max.z) bounds.max.z = p.z;
    }

    return bounds;
}





// Calculates the AABB box for a Skinned Mesh
static AABB CalculateAABB_Skinned(const Vertex3DSkinned* vertices, uint32_t vertex_count)
{
    AABB bounds = {
        (Vector3){ FLT_MAX, FLT_MAX, FLT_MAX },
        (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX}
    };

    for (uint32_t i = 0; i < vertex_count; i++)
    {
        Vector3 p = vertices[i].position;
        if (p.x < bounds.min.x) bounds.min.x = p.x;
        if (p.y < bounds.min.y) bounds.min.y = p.y;
        if (p.z < bounds.min.z) bounds.min.z = p.z;

        if (p.x > bounds.max.x) bounds.max.x = p.x;
        if (p.y > bounds.max.y) bounds.max.y = p.y;
        if (p.z > bounds.max.z) bounds.max.z = p.z;
    }

    return bounds;
}





// Expands an AABB symmetrically around its center (used as bind-pose fallback padding).
static AABB InflateAABBFromCenter(AABB bounds, float factor)
{
    Vector3 center = {
        (bounds.min.x + bounds.max.x) * 0.5f,
        (bounds.min.y + bounds.max.y) * 0.5f,
        (bounds.min.z + bounds.max.z) * 0.5f
    };

    Vector3 half = {
        (bounds.max.x - bounds.min.x) * 0.5f * factor,
        (bounds.max.y - bounds.min.y) * 0.5f * factor,
        (bounds.max.z - bounds.min.z) * 0.5f * factor
    };

    AABB out;
    out.min = (Vector3){ center.x - half.x, center.y - half.y, center.z - half.z };
    out.max = (Vector3){ center.x + half.x, center.y + half.y, center.z + half.z };
    return out;
}





// Creates a texture from memory
static Texture* Asset_CreateTextureFromMemory(const char* name, const unsigned char* buffer, int length)
{
    ImageData img = Image_LoadFromMemory(buffer, length, true);
    if (!img.pixels) return Asset_GetDefaultTexture();

    TextureHandle handle = Render_CreateTexture(renderer, img.pixels, img.width, img.height, img.channels);
    Image_Free(&img);

    // Cache it to not load the same texture twice
    if (texture_count < MAX_CACHED_TEXTURES)
    {
        Texture* t = &texture_cache[texture_count];
        strcpy(t->name, name);
        t->id = texture_count;
        t->gpu_handle = handle;
        t->width = img.width;
        t->height = img.height;
        t->channels = img.channels;

        texture_count++;

        return t;
    }

    return Asset_GetDefaultTexture();
}





// Helper to extract just the filename from a full path
static const char* GetBaseFilename(const char* path)
{
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* split = (last_slash > last_backslash) ? last_slash : last_backslash;
    return split ? split + 1 : path;
}





// Converts an Assimp matrix to a Column-Major matrix
static Matrix4 AssimpToColumnMatrix(struct aiMatrix4x4 m)
{
    Matrix4 m_out;
    
    m_out.m00 = m.a1; m_out.m01 = m.a2; m_out.m02 = m.a3; m_out.m03 = m.a4;
    m_out.m10 = m.b1; m_out.m11 = m.b2; m_out.m12 = m.b3; m_out.m13 = m.b4;
    m_out.m20 = m.c1; m_out.m21 = m.c2; m_out.m22 = m.c3; m_out.m23 = m.c4;
    m_out.m30 = m.d1; m_out.m31 = m.d2; m_out.m32 = m.d3; m_out.m33 = m.d4;
    
    return m_out;
}





// Recursively searches the node tree to find the accumulated transform for a specific mesh
static bool GetMeshGlobalTransform(struct aiNode* node, uint32_t mesh_index, Matrix4 parent_mat, Matrix4* out_mat, const char** out_node_name)
{
    Matrix4 local = AssimpToColumnMatrix(node->mTransformation);
    Matrix4 global = Matrix4Multiply(parent_mat, local);

    // If this node uses our target mesh, return it
    for (uint32_t i = 0; i < node->mNumMeshes; i++)
    {
        if (node->mMeshes[i] == mesh_index)
        {
            *out_mat = global;

            if (out_node_name != NULL)
                *out_node_name = node->mName.data;

            return true;
        }
    }

    // Otherwise, keep searching the children
    for (uint32_t i = 0; i < node->mNumChildren; i++)
    {
        if (GetMeshGlobalTransform(node->mChildren[i], mesh_index, global, out_mat, out_node_name))
        {
            return true;
        }
    }

    return false;
}





// Helper function to find or add a bone to a global Model Skeleton
static int GetOrAddBone(Skeleton* skeleton, const char* bone_name, Matrix4 inverse_bind)
{
    // Check if the bone already exists (from a previous sub-mesh)
    for (uint32_t i = 0; i < skeleton->bone_count; i++)
    {
        if (strcmp(skeleton->bones[i].name, bone_name) == 0)
            return i;
    }

    if (skeleton->bone_count >= MAX_BONES)
    {
        Log_Warning("Max Bones exceeded. Model might look broken");
        return 0;
    }

    int new_id = skeleton->bone_count;

    // Add new bone
    strncpy(skeleton->bones[new_id].name, bone_name, 63);
    skeleton->bones[new_id].name[63] = '\0';
    skeleton->bones[new_id].inverse_bind = inverse_bind;
    skeleton->bone_count++;

    return new_id;
}





// Copies a skeleton node tree into a SkeletonNode struct
static SkeletonNode* CopyAssimpNodeTree(struct aiNode* ai_node)
{
    if (!ai_node)
        return NULL;

    SkeletonNode* new_node = malloc(sizeof(SkeletonNode));
    strncpy(new_node->name, ai_node->mName.data, MAX_NAME_LENGTH-1);
    new_node->name[MAX_NAME_LENGTH-1] = '\0';
    new_node->default_local_transform = AssimpToColumnMatrix(ai_node->mTransformation);
    
    new_node->child_count = ai_node->mNumChildren;
    if (new_node->child_count > 0)
    {
        new_node->children = malloc(sizeof(SkeletonNode) * new_node->child_count);
        for (uint32_t i = 0; i < new_node->child_count; i++)
            new_node->children[i] = *CopyAssimpNodeTree(ai_node->mChildren[i]);
    }
    else
        new_node->children = NULL;
    
    return new_node;
}





// Main model loader function
Model* Asset_LoadModel(const char* name, const char* filepath)
{
    // Open the file in an aiScene
    const struct aiScene* scene = aiImportFile(filepath, 
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_LimitBoneWeights);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        Log_Error("ASSIMP ERROR: %s\n", aiGetErrorString());
        return NULL;
    }

    // Allocate the Model memory
    Model* new_model = malloc(sizeof(Model));
    strcpy(new_model->name, name);
    new_model->node_count = scene->mNumMeshes;
    new_model->nodes = malloc(sizeof(ModelNode) * new_model->node_count);

    new_model->skeleton = malloc(sizeof(Skeleton));
    new_model->skeleton->bone_count = 0;

    // Create a temporary array to map Assimp material indices to Prism MaterialHandles
    Material** material_map = malloc(sizeof(Material*) * scene->mNumMaterials);



    // ----- Loop through all textures -----
    
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        struct aiMaterial* ai_mat = scene->mMaterials[i];
        Texture* tex_ptr = Asset_GetDefaultTexture(); // Fallback
        Shader* model_shader = NULL;

        if (scene->mNumAnimations > 0)
            model_shader = NULL;

        // Ask Assimp if this material has a Diffuse Texture
        struct aiString tex_path;
        if (aiGetMaterialTexture(ai_mat, aiTextureType_DIFFUSE, 0, &tex_path, NULL, NULL, NULL, NULL, NULL, NULL) == aiReturn_SUCCESS)
        {
            const struct aiTexture* embedded_tex = NULL;

            // glTF Style case (starts with '*')
            if (tex_path.data[0] == '*')
            {
                int texture_index = atoi(&tex_path.data[1]);
                if (texture_index >= 0 && texture_index < scene->mNumTextures)
                {
                    embedded_tex = scene->mTextures[texture_index];
                }
            }
            // FBX Style (Match by Filename)
            else
            {
                const char* target_filename = GetBaseFilename(tex_path.data);
                
                for (uint32_t t = 0; t < scene->mNumTextures; t++)
                {
                    const char* embedded_filename = GetBaseFilename(scene->mTextures[t]->mFilename.data);
                    
                    if (strcmp(embedded_filename, target_filename) == 0)
                    {
                        embedded_tex = scene->mTextures[t];
                        break;
                    }
                }
            }

            // --- Load the Texture ---
            // If we found embedded data (either from glTF or FBX)
            if (embedded_tex != NULL)
            {
                if (embedded_tex->mHeight == 0) 
                {
                    char tex_name[256];
                    snprintf(tex_name, sizeof(tex_name), "%s_embedded_%s", name, tex_path.data);
                    tex_ptr = Asset_CreateTextureFromMemory(tex_name, (const unsigned char*)embedded_tex->pcData, embedded_tex->mWidth);
                }
                else 
                {
                    Log_Warning("Uncompressed embedded textures are not supported!");
                }
            }
            // If we didn't find embedded data, it must be an external file
            else
            {
                // External File Fallback
                char model_dir[512] = {0};
                strcpy(model_dir, filepath);
                
                char* last_slash = strrchr(model_dir, '/');
                char* last_backslash = strrchr(model_dir, '\\');
                char* split = (last_slash > last_backslash) ? last_slash : last_backslash;

                if (split)
                    *(split + 1) = '\0'; // Keep the trailing slash
                else
                    model_dir[0] = '\0'; // No directory found

                const char* clean_filename = GetBaseFilename(tex_path.data);

                // Path Strategy 1: Preserve Relative Subdirectories (For glTF)
                // e.g., "assets/SampleObjects/" + "Textures/colors.jpg"
                char try_path_1[1024];
                snprintf(try_path_1, sizeof(try_path_1), "%s%s", model_dir, tex_path.data);

                // PATH STRATEGY 2: Flattened Filename (For FBX)
                // e.g., "assets/SampleObjects/" + "colors.jpg"
                char try_path_2[1024];
                snprintf(try_path_2, sizeof(try_path_2), "%s%s", model_dir, clean_filename);

                // Check which path actually exists on the hard drive
                FILE* f1 = fopen(try_path_1, "rb");
                if (f1) 
                {
                    fclose(f1);
                    tex_ptr = Asset_LoadTexture(clean_filename, try_path_1);
                }
                else 
                {
                    FILE* f2 = fopen(try_path_2, "rb");
                    if (f2) 
                    {
                        fclose(f2);
                        tex_ptr = Asset_LoadTexture(clean_filename, try_path_2);
                    }
                    else 
                    {
                        // Neither path worked. File is missing.
                        Log_Warning("ASSIMP: Could not find texture %s", clean_filename);
                    }
                }
            }
        }

        // Extract Material Color
        struct aiColor4D diffuse_color = {1.0f, 1.0f, 1.0f, 1.0f}; // Default to white
        aiGetMaterialColor(ai_mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse_color);

        // Create the material
        Material* mat_ptr = Asset_CreateMaterial(model_shader, tex_ptr);
        
        // Update the tint color to match the 3D file
        if (mat_ptr)
        {
            mat_ptr->properties.albedo_tint = (Color){ diffuse_color.r, diffuse_color.g, diffuse_color.b, diffuse_color.a };

            float metallic_val = 0.0f;
            if (aiGetMaterialFloatArray(ai_mat, "$mat.gltf.pbrMetallicRoughness.metallicFactor", 0, 0, &metallic_val, NULL) == aiReturn_SUCCESS)
                mat_ptr->properties.metallic_factor = metallic_val;
            else if (aiGetMaterialFloatArray(ai_mat, AI_MATKEY_METALLIC_FACTOR, &metallic_val, NULL) == aiReturn_SUCCESS)
                mat_ptr->properties.metallic_factor = metallic_val;

            float roughness_val = 0.5f;
            if (aiGetMaterialFloatArray(ai_mat, "$mat.gltf.pbrMetallicRoughness.roughnessFactor", 0, 0, &roughness_val, NULL) == aiReturn_SUCCESS)
                mat_ptr->properties.roughness_factor = roughness_val;
            else if (aiGetMaterialFloatArray(ai_mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness_val, NULL) == aiReturn_SUCCESS)
                mat_ptr->properties.roughness_factor = roughness_val;
        }

        material_map[i] = mat_ptr;
    }


    // ----- Loop through every piece of the file -----
    
    for (uint32_t m = 0; m < scene->mNumMeshes; m++)
    {
        struct aiMesh* ai_mesh = scene->mMeshes[m];
        
        uint32_t vertex_count = ai_mesh->mNumVertices;
        uint32_t max_index_count = ai_mesh->mNumFaces * 3; 
        uint32_t* indices = malloc(sizeof(uint32_t) * max_index_count);

        // Get the nodes transform
        Matrix4 node_transform = Matrix4Identity();
        const char* real_node_name = "UnknownNode";
        GetMeshGlobalTransform(scene->mRootNode, m, Matrix4Identity(), &node_transform, &real_node_name);

        GetOrAddBone(new_model->skeleton, real_node_name, Matrix4Inverse(node_transform));

        // Check if the file contains animations
        bool has_animations = (scene->mNumAnimations > 0);
        bool is_skinned = (ai_mesh->mNumBones > 0);
        
        new_model->nodes[m].is_skinned = is_skinned;
        new_model->nodes[m].material = material_map[ai_mesh->mMaterialIndex];


        // Skinned meshes
        if (is_skinned)
        {
            if (skinned_mesh_count >= MAX_CACHED_SKINNED_MESHES)
            {
                Log_Error("CRITICAL: Skinned mesh cache limit reached!");
                continue;
            }

            Vertex3DSkinned* vertices = malloc(sizeof(Vertex3DSkinned) * vertex_count);

            // 1. Extract Vertices
            for (uint32_t i = 0; i < vertex_count; i++)
            {
                Vertex3DSkinned v;
                v.position = (Vector3){ ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
                v.normal = (Vector3){ ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };
                
                if (ai_mesh->mTextureCoords[0]) v.uv = (Vector2){ ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
                else v.uv = (Vector2){ 0.0f, 0.0f };

                if (ai_mesh->mTangents)
                    v.tangent = (Vector3){ ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z };
                else
                    v.tangent = (Vector3){ 0.0f, 0.0f, 0.0f };

                // Initialize bones to default (empty) state
                for (int b = 0; b < MAX_BONE_INFLUENCE; b++)
                {
                    v.bone_ids[b] = -1;
                    v.bone_weights[b] = 0.0f;
                }
                vertices[i] = v;
            }

            // 2. Extract Bone Weights
            for (uint32_t b = 0; b < ai_mesh->mNumBones; b++)
            {
                struct aiBone* ai_bone = ai_mesh->mBones[b];
                Matrix4 inv_bind = AssimpToColumnMatrix(ai_bone->mOffsetMatrix);
                int bone_id = GetOrAddBone(new_model->skeleton, ai_bone->mName.data, inv_bind);

                for (uint32_t w = 0; w < ai_bone->mNumWeights; w++)
                {
                    uint32_t vertex_id = ai_bone->mWeights[w].mVertexId;
                    float weight = ai_bone->mWeights[w].mWeight;

                    for (int slot = 0; slot < MAX_BONE_INFLUENCE; slot++)
                    {
                        if (vertices[vertex_id].bone_ids[slot] == -1) {
                            vertices[vertex_id].bone_ids[slot] = bone_id;
                            vertices[vertex_id].bone_weights[slot] = weight;
                            break;
                        }
                    }
                }
            }

            // 3. Extract Indices
            uint32_t actual_index_count = 0;
            for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++) {
                struct aiFace face = ai_mesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++) indices[actual_index_count++] = face.mIndices[j];
            }

            // 4. Send to GPU and Cache
            MeshHandle mesh_handle = Render_CreateSkinnedMesh(renderer, vertices, vertex_count, indices, actual_index_count);
            SkinnedMesh* sub_mesh = &skinned_mesh_cache[skinned_mesh_count];
            
            strncpy(sub_mesh->name, real_node_name, sizeof(sub_mesh->name) - 1);
            sub_mesh->name[sizeof(sub_mesh->name) - 1] = '\0';
            sub_mesh->id = skinned_mesh_count;
            sub_mesh->gpu_handle = mesh_handle;
            sub_mesh->vertices = vertices;
            sub_mesh->vertex_count = vertex_count;
            sub_mesh->indices = indices;
            sub_mesh->index_count = actual_index_count;
            sub_mesh->local_bounds = CalculateAABB_Skinned(vertices, vertex_count);

            // TODO: Make animations change the local bounds of AABB's
            // Inflate bounds for animations to prevent premature culling
            sub_mesh->local_bounds = InflateAABBFromCenter(sub_mesh->local_bounds, 2.0f);

            skinned_mesh_count++;
            new_model->nodes[m].skinned_mesh = sub_mesh;
            new_model->nodes[m].mesh = NULL;
        }

        // Static meshes
        else 
        {
            if (mesh_count >= MAX_CACHED_MESHES)
            {
                Log_Error("CRITICAL: Static mesh cache limit reached!");
                continue; // Skip loading this sub-mesh to prevent a crash
            }

            Vertex3D* vertices = malloc(sizeof(Vertex3D) * vertex_count);

            // 1. Extract Vertices
            for (uint32_t i = 0; i < vertex_count; i++)
            {
                Vertex3D v;
                Vector3 raw_pos = (Vector3){ ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
                Vector3 raw_norm = (Vector3){ ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };

                Vector3 raw_tan = (Vector3){ 0.0f, 0.0f, 0.0f };
                if (ai_mesh->mTangents)
                    raw_tan = (Vector3){ ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z };

                // Apply node transformation directly to geometry if rigid
                if (!has_animations)
                {
                    v.position = Matrix4MultiplyVector3(node_transform, raw_pos);
                    v.normal.x = node_transform.m00 * raw_norm.x + node_transform.m01 * raw_norm.y + node_transform.m02 * raw_norm.z;
                    v.normal.y = node_transform.m10 * raw_norm.x + node_transform.m11 * raw_norm.y + node_transform.m12 * raw_norm.z;
                    v.normal.z = node_transform.m20 * raw_norm.x + node_transform.m21 * raw_norm.y + node_transform.m22 * raw_norm.z;

                    v.tangent.x = node_transform.m00 * raw_tan.x + node_transform.m01 * raw_tan.y + node_transform.m02 * raw_tan.z;
                    v.tangent.y = node_transform.m10 * raw_tan.x + node_transform.m11 * raw_tan.y + node_transform.m12 * raw_tan.z;
                    v.tangent.z = node_transform.m20 * raw_tan.x + node_transform.m21 * raw_tan.y + node_transform.m22 * raw_tan.z;

                    float length = sqrtf(v.normal.x*v.normal.x + v.normal.y*v.normal.y + v.normal.z*v.normal.z);
                    if (length > 0.0001f)
                    {
                        v.normal.x /= length; v.normal.y /= length; v.normal.z /= length;
                    }

                    float tan_length = sqrtf(v.tangent.x*v.tangent.x + v.tangent.y*v.tangent.y + v.tangent.z*v.tangent.z);
                    if (tan_length > 0.0001f)
                    {
                        v.tangent.x /= tan_length; v.tangent.y /= tan_length; v.tangent.z /= tan_length;
                    }
                }
                else
                {
                    v.position = raw_pos;
                    v.normal = raw_norm;
                    v.tangent = raw_tan;
                }

                if (ai_mesh->mTextureCoords[0]) v.uv = (Vector2){ ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
                else v.uv = (Vector2){ 0.0f, 0.0f };

                float tan_length = sqrtf(v.tangent.x*v.tangent.x + v.tangent.y*v.tangent.y + v.tangent.z*v.tangent.z);
                if (tan_length > 0.0001f)
                {
                    v.tangent.x /= tan_length; v.tangent.y /= tan_length; v.tangent.z /= tan_length;
                }
                else
                {
                    Vector3 n = v.normal;
                    Vector3 c = (fabsf(n.z) < 0.99f) ? (Vector3){ -n.y, n.x, 0.0f } : (Vector3){ 0.0f, -n.z, n.y };
                    float l = sqrtf(c.x*c.x + c.y*c.y + c.z*c.z);
                    v.tangent = (l > 0.0001f) ? (Vector3){ c.x/l, c.y/l, c.z/l } : (Vector3){ 1.0f, 0.0f, 0.0f };
                }

                vertices[i] = v;
            }

            // 2. Extract Indices
            uint32_t actual_index_count = 0;
            for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++) {
                struct aiFace face = ai_mesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++) indices[actual_index_count++] = face.mIndices[j];
            }

            // 3. Send to GPU and Cache
            MeshHandle mesh_handle = Render_CreateMesh(renderer, vertices, vertex_count, indices, actual_index_count);
            Mesh* sub_mesh = &mesh_cache[mesh_count];
            
            strncpy(sub_mesh->name, real_node_name, sizeof(sub_mesh->name) - 1);
            sub_mesh->name[sizeof(sub_mesh->name) - 1] = '\0';
            sub_mesh->id = mesh_count;
            sub_mesh->gpu_handle = mesh_handle;
            sub_mesh->vertices = vertices;
            sub_mesh->vertex_count = vertex_count;
            sub_mesh->indices = indices;
            sub_mesh->index_count = actual_index_count;
            sub_mesh->local_bounds = CalculateAABB_Static(vertices, vertex_count);

            mesh_count++;
            new_model->nodes[m].mesh = sub_mesh;
            new_model->nodes[m].skinned_mesh = NULL;
        }
    }

    // Sets the root of the skeleton node
    new_model->skeleton->root_node = CopyAssimpNodeTree(scene->mRootNode);



    // ----- Extract Animations -----

    new_model->animation_count = scene->mNumAnimations;
    
    if (new_model->animation_count > 0)
    {
        // Allocate array of pointers
        new_model->animations = malloc(sizeof(AnimationClip*) * new_model->animation_count);

        for (uint32_t a = 0; a < scene->mNumAnimations; a++)
        {
            struct aiAnimation* ai_anim = scene->mAnimations[a];
            AnimationClip* clip = malloc(sizeof(AnimationClip));

            strncpy(clip->name, ai_anim->mName.data, MAX_NAME_LENGTH-1);
            clip->name[MAX_NAME_LENGTH-1] = '\0';
            clip->duration_ticks = (float)ai_anim->mDuration;

            if (ai_anim->mTicksPerSecond != 0)
                clip->ticks_per_second = (float)ai_anim->mTicksPerSecond;
            else
            {
                if (clip->duration_ticks < 100.0f)
                    clip->ticks_per_second = 1.0f; // 1 tick = 1 second
                else
                    clip->ticks_per_second = 24.0f;
            }
            
            
            
            clip->channel_count = ai_anim->mNumChannels;
            clip->channels = malloc(sizeof(AnimationChannel) * clip->channel_count);
            
            for (uint32_t c = 0; c < clip->channel_count; c++)
            {
                struct aiNodeAnim* ai_channel = ai_anim->mChannels[c];
                AnimationChannel* channel = &clip->channels[c];
                
                strncpy(channel->bone_name, ai_channel->mNodeName.data, 63);
                channel->bone_name[63] = '\0';
                
                // Extract Positions
                channel->position_count = ai_channel->mNumPositionKeys;
                channel->positions = malloc(sizeof(VectorKey) * channel->position_count);
                for (uint32_t k = 0; k < channel->position_count; k++) 
                {
                    channel->positions[k].time = (float)ai_channel->mPositionKeys[k].mTime;
                    channel->positions[k].value = (Vector3){ ai_channel->mPositionKeys[k].mValue.x, ai_channel->mPositionKeys[k].mValue.y, ai_channel->mPositionKeys[k].mValue.z };
                }
                
                // Extract Rotations (Assimp Quaternions are W, X, Y, Z order)
                channel->rotation_count = ai_channel->mNumRotationKeys;
                channel->rotations = malloc(sizeof(QuaternionKey) * channel->rotation_count);
                for (uint32_t k = 0; k < channel->rotation_count; k++)
                {
                    channel->rotations[k].time = (float)ai_channel->mRotationKeys[k].mTime;
                    channel->rotations[k].value = (Quaternion){ ai_channel->mRotationKeys[k].mValue.x, ai_channel->mRotationKeys[k].mValue.y, ai_channel->mRotationKeys[k].mValue.z, ai_channel->mRotationKeys[k].mValue.w };
                }
                
                // Extract Scales
                channel->scale_count = ai_channel->mNumScalingKeys;
                channel->scales = malloc(sizeof(VectorKey) * channel->scale_count);
                for (uint32_t k = 0; k < channel->scale_count; k++)
                {
                    channel->scales[k].time = (float)ai_channel->mScalingKeys[k].mTime;
                    channel->scales[k].value = (Vector3){ ai_channel->mScalingKeys[k].mValue.x, ai_channel->mScalingKeys[k].mValue.y, ai_channel->mScalingKeys[k].mValue.z };
                }
            }
            
            new_model->animations[a] = clip;
        }
    }
    else
    {
        new_model->animations = NULL;
    }
        
    free(material_map);

    // Free Assimp's memory
    aiReleaseImport(scene);

    if (model_count < MAX_CACHED_MODELS)
        model_cache[model_count++] = new_model;
    else
        Log_Warning("MAX_CACHED_MODESL reached");

    return new_model;
}





// Loads a mesh from a filepath (uses fastobj for obj files)
Mesh* Asset_LoadMesh(const char* name, const char* filepath)
{
    // Check if mesh limit has been reached
    for (uint32_t i = 0; i < mesh_count; i++)
    {
        if (strcmp(mesh_cache[i].name, name) == 0)
            return &mesh_cache[i];
    }

    // Use fast_obj to read file (fast_obj automatically triangulates quads)
    fastObjMesh* mesh = fast_obj_read(filepath);
    if (!mesh)
    {
        Log_Warning("CRITICAL: Failed to load OBJ: %s\n", filepath);
        return NULL;
    }

    uint32_t total_triangles = 0;
    for (unsigned int i = 0; i < mesh->face_count; ++i)
    {
        // A face with N vertices creates (N - 2) triangles.
        // E.g., a Quad (4) makes 2 triangles. A Triangle (3) makes 1.
        total_triangles += (mesh->face_vertices[i] - 2);
    }
    
    uint32_t vertex_count = total_triangles * 3;
    uint32_t index_count = vertex_count;

    Vertex3D* final_vertices = malloc(sizeof(Vertex3D) * vertex_count);
    uint32_t* final_indices  = malloc(sizeof(uint32_t) * index_count);

    // --- Triangulation Loop ---
    uint32_t vertex_offset = 0;
    uint32_t index_offset = 0; // Where we are in the fast_obj index array

    for (unsigned int i = 0; i < mesh->face_count; ++i)
    {
        unsigned int fv = mesh->face_vertices[i]; // How many vertices in this face?

        // "Triangle Fan" algorithm to split quads/n-gons into triangles
        for (unsigned int j = 1; j < fv - 1; ++j)
        {
            // We always use vertex 0, then j, then j+1 to build the triangle
            fastObjIndex indices[3] = {
                mesh->indices[index_offset + 0],
                mesh->indices[index_offset + j],
                mesh->indices[index_offset + j + 1]
            };

            // Extract the 3 vertices for this specific triangle
            for (int k = 0; k < 3; ++k)
            {
                fastObjIndex mi = indices[k];
                Vertex3D v;

                v.tangent = (Vector3){0.0f, 0.0f, 0.0f};

                v.position.x = mesh->positions[mi.p * 3 + 0];
                v.position.y = mesh->positions[mi.p * 3 + 1];
                v.position.z = mesh->positions[mi.p * 3 + 2];

                if (mi.t)
                {
                    v.uv.x = mesh->texcoords[mi.t * 2 + 0];
                    v.uv.y = mesh->texcoords[mi.t * 2 + 1];
                }
                else
                {
                    v.uv = (Vector2){0.0f, 0.0f};
                }

                if (mi.n)
                {
                    v.normal.x = mesh->normals[mi.n * 3 + 0];
                    v.normal.y = mesh->normals[mi.n * 3 + 1];
                    v.normal.z = mesh->normals[mi.n * 3 + 2];
                }
                else
                {
                    v.normal = (Vector3){0.0f, 1.0f, 0.0f};
                }

                final_vertices[vertex_offset] = v;
                final_indices[vertex_offset]  = vertex_offset;
                vertex_offset++;
            }
        }
        
        index_offset += fv; 
    }

    Mesh_CalculateVertexTangents(final_vertices, vertex_count, final_indices, index_count);


    // Create GPU mesh
    MeshHandle handle = Render_CreateMesh(renderer, final_vertices, vertex_count, final_indices, index_count);

    // Check if we've reached the maximum cached meshes
    if (mesh_count >= MAX_CACHED_MESHES)
    {
        Log_Warning("MAX_CACHED_MESHES reached. Cannot cache %s", name);
        free(final_vertices);
        free(final_indices);
        fast_obj_destroy(mesh);
        return NULL;
    }

    Mesh* new_mesh = &mesh_cache[mesh_count];

    // Cache mesh
    strcpy(new_mesh->name, name);
    new_mesh->gpu_handle = handle;

    new_mesh->id = mesh_count;
    new_mesh->index_count = index_count;
    new_mesh->indices = final_indices;
    new_mesh->local_bounds = CalculateAABB_Static(final_vertices, vertex_count);
    new_mesh->vertex_count = vertex_count;
    new_mesh->vertices = final_vertices;

    mesh_count++;

    fast_obj_destroy(mesh);


    return new_mesh;
}





// Creates a texture from a color
Texture* Asset_CreateSolidColorTexture(const char* name, Color color)
{
    // Check if this specific color texture was already generated
    for (uint32_t i = 0; i < texture_count; i++)
    {
        if (strcmp(texture_cache[i].name, name) == 0)
            return &texture_cache[i]; 
    }

    // Convert the float (0.0 - 1.0) Color into 8-bit (0 - 255) pixel data
    unsigned char pixel[4] = {
        (unsigned char)(color.r * 255.0f),
        (unsigned char)(color.g * 255.0f),
        (unsigned char)(color.b * 255.0f),
        (unsigned char)(color.a * 255.0f)
    };

    // Create the texture from the renderer
    TextureHandle handle = Render_CreateTexture(renderer, pixel, 1, 1, 4);

    // Cache it exactly like a normal texture
    if (texture_count < MAX_CACHED_TEXTURES)
    {
        Texture* t = &texture_cache[texture_count];
        strcpy(t->name, name);
        t->id = texture_count;
        t->gpu_handle = handle;
        t->width = 1;
        t->height = 1;
        t->channels = 4;
        
        texture_count++;
        return t;
    }
    
    return Asset_GetDefaultTexture();
}





// Creates a material from a given shader and texture (diffuse)
Material* Asset_CreateMaterial(Shader* shader, Texture* albedo)
{
    if (material_count >= MAX_MATERIALS)
    {
        Log_Error("Material pool full!");
        return NULL;
    }
    
    uint32_t id = material_count++;
    Material* mat = &material_pool[id];

    mat->id = id;
    mat->active = true;

    mat->shader = shader;
    
    if (albedo == NULL)
        mat->albedo_texture = Asset_GetDefaultTexture();
    else
        mat->albedo_texture = albedo;
    
    // Set other textures to NULL
    mat->normal_map = NULL;
    mat->metallic_map = NULL;
    mat->roughness_map = NULL;

    // Default physical properties
    mat->properties.albedo_tint = (Color){1.0f, 1.0f, 1.0f, 1.0f}; // Pure white
    mat->properties.metallic_factor = 0.0f;                        // Standard plastic
    mat->properties.roughness_factor = 0.5f;                       // Medium reflection
    
    
    return mat;
}





// Creates a dynamic mesh from the renderer and returns the handle to the mesh
Mesh* Asset_CreateDynamicMesh(uint32_t max_vertices, uint32_t max_indices)
{
    if (mesh_count >= MAX_CACHED_MESHES)
    {
        Log_Error("Cannot create dynamic mesh. MAX_CACHED_MESHES reached.");
        return NULL;
    }

    MeshHandle handle = Render_CreateDynamicMesh(renderer, max_vertices, max_indices);

    Mesh* dynamic_mesh = &mesh_cache[mesh_count];

    strncpy(dynamic_mesh->name, "Dynamic_Mesh", MAX_NAME_LENGTH - 1);
    dynamic_mesh->name[MAX_NAME_LENGTH - 1] = '\0';

    dynamic_mesh->id = mesh_count;
    dynamic_mesh->gpu_handle = handle;

    // TODO: Add proper local bounds to dynamic meshes when the renderer gets upgraded
    dynamic_mesh->local_bounds.min = (Vector3){-99999.0f, -99999.0f, -99999.0f};
    dynamic_mesh->local_bounds.max = (Vector3){ 99999.0f,  99999.0f,  99999.0f};
    dynamic_mesh->vertices = NULL;
    dynamic_mesh->vertex_count = 0;
    dynamic_mesh->indices = NULL;
    dynamic_mesh->index_count = 0;

    mesh_count++;
    
    return dynamic_mesh;
}





// Updates a dynamic mesh from the renderer
void Asset_UpdateDynamicMesh(Mesh* mesh, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
{
    if (!mesh)
        return;

    mesh->vertex_count = vertex_count;
    mesh->index_count = index_count;

    Render_UpdateDynamicMesh(renderer, mesh->gpu_handle, vertices, vertex_count, indices, index_count);
}





// Loads a vertex and fragment shader from a file path and compiles it to a full shader
Shader* Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path)
{
    // Check if we already loaded this exact shader
    for (uint32_t i = 0; i < shader_count; i++)
    {
        if (strcmp(shader_cache[i].name, name) == 0)
        {
            return &shader_cache[i];
        }
    }

    // Check if max shader count has been reached
    if (shader_count >= MAX_CACHED_SHADERS)
    {
        Log_Error("CRITICAL: Asset Manager out of shader cache space!\n");
        return NULL;
    }


    // Read shaders from file
    char* v_src = IO_ReadTextFile(vert_path);
    char* f_src = IO_ReadTextFile(frag_path);


    if (!v_src || !f_src)
    {
        Log_Error("CRITICAL: Failed to read shader files for '%s'\n", name);
        if (v_src) free(v_src);
        if (f_src) free(f_src);
        return NULL;
    }

    // Compile on GPU
    ShaderHandle new_handle = Render_CreateShader(renderer, v_src, f_src);

    // Free shaders from memory
    free(v_src);
    free(f_src);

    Shader* new_shad = &shader_cache[shader_count];
    strcpy(new_shad->name, name);
    new_shad->id = shader_count;
    new_shad->gpu_handle = new_handle;
    shader_count++;

    return new_shad;
}





// Loads a texture from a filepath
Texture* Asset_LoadTexture(const char* name, const char* filepath)
{
    // Check if the texture is already loaded
    for (uint32_t i = 0; i < texture_count; i++)
    {
        if (strcmp(texture_cache[i].name, name) == 0)
        {
            return &texture_cache[i];
        }
    }

    // Load pixels
    ImageData img = Image_Load(filepath, true);
    if (!img.pixels)
    {
        Log_Error("CRITICAL: Failed to load texture: %s\n", filepath);
        return NULL;
    }

    // Create mesh on GPU
    TextureHandle handle = Render_CreateTexture(renderer, img.pixels, img.width, img.height, img.channels);

    // Free image from memory
    Image_Free(&img);

    Texture* new_text = &texture_cache[texture_count];
    strcpy(new_text->name, name);
    new_text->id = texture_count;
    new_text->gpu_handle = handle;
    new_text->width = img.width;
    new_text->height = img.height;
    new_text->channels = img.channels;

    texture_count++;

    return new_text;
}





// Loads a skybox texture from 6 separate images
Texture* Asset_LoadSkyboxTexture(const char* name, const char* right, const char* left, const char* top, const char* bottom, const char* front, const char* back)
{
    // Check if it's already loaded
    for (uint32_t i = 0; i < texture_count; i++)
    {
        if (strcmp(texture_cache[i].name, name) == 0)
            return &texture_cache[i];
    }

    // Load all 6 images
    const char* paths[6] = { right, left, top, bottom, front, back };
    ImageData images[6];
    bool failed = false;

    
    for (int i = 0; i < 6; i++)
    {
        images[i] = Image_Load(paths[i], false);
        if (!images[i].pixels)
        {
            Log_Error("CRITICAL: Failed to load skybox face: %s\n", paths[i]);
            failed = true;
            break; // Stop loading if one fails
        }
    }

    // If any image failed, clean up the successful ones and abort
    if (failed)
    {
        for (int i = 0; i < 6; i++) {
            if (images[i].pixels) Image_Free(&images[i]);
        }
        return NULL;
    }

    // Image_Rotate90CW(&images[2]);

    // Send all 6 pixel arrays to the GPU (Assuming all 6 faces of a skybox have the exact same width/height/channels)
    TextureHandle handle = Render_CreateCubemap(
        renderer,
        images[0].pixels, // 0: Right
        images[1].pixels, // 1: Left
        images[2].pixels, // 2: Top
        images[3].pixels, // 3: Bottom
        images[4].pixels, // 4: Front
        images[5].pixels, // 5: Back
        images[0].width, images[0].height, images[0].channels
    );

    // Free the RAM for all 6 images
    for (int i = 0; i < 6; i++)
        Image_Free(&images[i]);

    // Save to cache
    Texture* new_text = &texture_cache[texture_count];
    strcpy(new_text->name, name);
    new_text->id = texture_count;
    new_text->gpu_handle = handle;
    new_text->width = images[0].width;
    new_text->height = images[0].height;
    new_text->channels = images[0].channels;

    texture_count++;

    return new_text;
}










// Generate built in quad mesh (basic square)
Mesh* Asset_GetBuiltinQuad()
{
    // If we already made the quad, return the ID
    if (builtin_quad != NULL)
    {
        return builtin_quad;
    }

    // 4 corners, 2 triangles (6 indices)
    Vertex3D vertices[] = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // Bottom-Left
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // Bottom-Right
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Top-Right
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}  // Top-Left
    };

    uint32_t indices[] = {
        0, 1, 2, // Triangle 1
        2, 3, 0  // Triangle 2
    };

    Mesh_CalculateVertexTangents(vertices, 4, indices, 6);


    // Copy vertices and indices to the heap for caching
    Vertex3D* heap_vertices = malloc(sizeof(vertices));
    memcpy(heap_vertices, vertices, sizeof(vertices));
    uint32_t* heap_indices = malloc(sizeof(indices));
    memcpy(heap_indices, indices, sizeof(indices));

    MeshHandle gpu_handle = Render_CreateMesh(renderer, heap_vertices, 4, heap_indices, 6);

    // Cache the mesh
    if (mesh_count < MAX_CACHED_MESHES)
    {
        Mesh* m = &mesh_cache[mesh_count];
        strcpy(m->name, "Quad");
        m->gpu_handle = gpu_handle;

        m->vertices = heap_vertices;
        m->indices = heap_indices;
        m->vertex_count = 4;
        m->index_count = 6;
        m->local_bounds = CalculateAABB_Static(heap_vertices, 4);

        builtin_quad = m;

        mesh_count++;
    }
    else
    {
        free(heap_vertices);
        free(heap_indices);
    }

    return builtin_quad;
}





// Generate a standard cube mesh
Mesh* Asset_GetBuiltinCube()
{
    if (builtin_cube != NULL)
        return builtin_cube;


    // 24 Vertices (4 per face for sharp normals and proper UVs)
    Vertex3D vertices[24] = {
        // Front face (Z =  0.5)
        {{-0.5f, -0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
        // Back face (Z = -0.5)
        {{ 0.5f, -0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
        // Left face (X = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f,  0.0f}, {0.0f, 1.0f}},
        // Right face (X =  0.5)
        {{ 0.5f, -0.5f,  0.5f}, {1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        // Top face (Y =  0.5)
        {{-0.5f,  0.5f,  0.5f}, {0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
        // Bottom face (Y = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
    };

    uint32_t indices[36] = {
         0,  1,  2,  2,  3,  0, // Front
         4,  5,  6,  6,  7,  4, // Back
         8,  9, 10, 10, 11,  8, // Left
        12, 13, 14, 14, 15, 12, // Right
        16, 17, 18, 18, 19, 16, // Top
        20, 21, 22, 22, 23, 20  // Bottom
    };

    Mesh_CalculateVertexTangents(vertices, 24, indices, 36);


    // Copy vertices and indices to the heap for caching
    Vertex3D* heap_vertices = malloc(sizeof(vertices));
    memcpy(heap_vertices, vertices, sizeof(vertices));
    uint32_t* heap_indices = malloc(sizeof(indices));
    memcpy(heap_indices, indices, sizeof(indices));


    MeshHandle gpu_handle = Render_CreateMesh(renderer, heap_vertices, 24, heap_indices, 36);
    
    // Cache the mesh
    if (mesh_count < MAX_CACHED_MESHES)
    {
        Mesh* m = &mesh_cache[mesh_count];
        strcpy(m->name, "Cube");
        m->id = mesh_count;
        m->gpu_handle = gpu_handle;

        m->vertices = heap_vertices;
        m->indices = heap_indices;
        m->vertex_count = 24;
        m->index_count = 36;
        m->local_bounds = CalculateAABB_Static(heap_vertices, 24);

        builtin_cube = m;

        mesh_count++;
    }
    else
    {
        free(heap_vertices);
        free(heap_indices);
    }

    return builtin_cube;
}





// Create a standard sphere
Mesh* Asset_GetBuiltinSphere()
{
    if (builtin_sphere != NULL)
        return builtin_sphere;

    // The resolution of the sphere. Higher = smoother but more memory.
    const int X_SEGMENTS = 32;
    const int Y_SEGMENTS = 32;
    const float PI = 3.14159265359f;

    uint32_t vertex_count = (X_SEGMENTS + 1) * (Y_SEGMENTS + 1);
    uint32_t index_count = X_SEGMENTS * Y_SEGMENTS * 6;

    Vertex3D* vertices = malloc(vertex_count * sizeof(Vertex3D));
    uint32_t* indices = malloc(index_count * sizeof(uint32_t));


    // Generate vertices
    uint32_t v_idx = 0;
    for (int y = 0; y <= Y_SEGMENTS; ++y)
    {
        for (int x = 0; x <= X_SEGMENTS; ++x)
        {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            
            // Spherical coordinate math
            float xPos = cosf(xSegment * 2.0f * PI) * sinf(ySegment * PI);
            float yPos = cosf(ySegment * PI);
            float zPos = sinf(xSegment * 2.0f * PI) * sinf(ySegment * PI);

            Vertex3D v;

            v.position = (Vector3){xPos * 0.5f, yPos * 0.5f, zPos * 0.5f}; // Radius 0.5
            v.normal = (Vector3){xPos, yPos, zPos}; // Normal is just the normalized position
            v.uv = (Vector2){xSegment, ySegment};

            vertices[v_idx++] = v;
        }
    }


    // Generate indices
    uint32_t i_idx = 0;
    for (int y = 0; y < Y_SEGMENTS; ++y)
    {
        for (int x = 0; x < X_SEGMENTS; ++x)
        {
            uint32_t current = y * (X_SEGMENTS + 1) + x;
            uint32_t next = current + X_SEGMENTS + 1;

            // Triangle 1
            indices[i_idx++] = current;
            indices[i_idx++] = current + 1;
            indices[i_idx++] = next + 1;
            
            // Triangle 2
            indices[i_idx++] = current;
            indices[i_idx++] = next + 1;
            indices[i_idx++] = next;
        }
    }

    Mesh_CalculateVertexTangents(vertices, vertex_count, indices, index_count);


    // Create mesh and cache it if possible
    MeshHandle gpu_handle = Render_CreateMesh(renderer, vertices, vertex_count, indices, index_count);

    if (mesh_count < MAX_CACHED_MESHES)
    {
        Mesh* m = &mesh_cache[mesh_count];
        strcpy(m->name, "Sphere");
        m->gpu_handle = gpu_handle;

        m->vertices = vertices;
        m->indices = indices;
        m->vertex_count = vertex_count;
        m->index_count = index_count;
        m->local_bounds = CalculateAABB_Static(vertices, vertex_count);

        builtin_sphere = m;

        mesh_count++;
    }
    else
    {
        free(vertices);
        free(indices);
    }

    return builtin_sphere;
}











// Create the default texture (all white)
Texture* Asset_GetDefaultTexture()
{
    // If already made, return the handle
    if (default_texture != NULL)
        return default_texture;

    TextureHandle handle = (TextureHandle){ 1 };

    if (texture_count < MAX_CACHED_TEXTURES)
    {
        Texture* t = &texture_cache[texture_count];
        strcpy(t->name, "DefaultTexture");
        t->id = texture_count;
        t->gpu_handle = handle;
        t->width = 1;
        t->height = 1;
        t->channels = 4;

        default_texture = t;

        texture_count++;
    }


    return default_texture;
}















// Look up a model by its string name
Model* Asset_GetModelByName(const char* name)
{
    for (uint32_t i = 0; i < model_count; i++)
    {
        if (strcmp(model_cache[i]->name, name) == 0)
            return model_cache[i];
    }

    return NULL;
}





// Look up a mesh by its string name
Mesh* Asset_GetMeshByName(const char* name)
{
    for (uint32_t i = 0; i < mesh_count; i++)
        if (strcmp(mesh_cache[i].name, name) == 0) return &mesh_cache[i];

    return NULL;
}





// Look up a skinned mesh by its string name
SkinnedMesh* Asset_GetSkinnedMeshByName(const char* name)
{
    for (uint32_t i = 0; i < skinned_mesh_count; i++)
        if (strcmp(skinned_mesh_cache[i].name, name) == 0) return &skinned_mesh_cache[i];

    return NULL;
}





// Look up a texture by its string name
Texture* Asset_GetTextureByName(const char* name)
{
    for (uint32_t i = 0; i < texture_count; i++)
        if (strcmp(texture_cache[i].name, name) == 0) return &texture_cache[i];

    return NULL;
}





// MeshData* Asset_GetMeshData(MeshHandle handle)
// {
//     // Search the cache for the matching handle and return the pointer
//     for (uint32_t i = 0; i < mesh_count; i++)
//     {
//         if (mesh_cache[i].handle.id == handle.id)
//         {
//             return &mesh_cache[i].mesh_data;
//         }
//     }
//     return NULL; // return NULL if not found
// }