#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "model_loader.h"
#include "asset_manager.h"
#include "../core/mesh_core.h"
#include "../core/log_core.h"
#include "../render/render.h"





// Checks if two strings are equal while being case-insensitive
static bool StrEqualsIgnoreCase(const char* a, const char* b)
{
    if (!a || !b)
    	return false;
    
    while (*a && *b)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return false;
        
        a++;
        b++;
    }
    
    return (*a == '\0' && *b == '\0');
}





// Extracts the file extension including the dot (e.g. ".fbx")
static const char* GetFileExtension(const char* filepath)
{
    if (!filepath)
    	return "";
    
    const char* dot = strrchr(filepath, '.');
    return dot ? dot : "";
}





// Extracts the filename from a full path
static const char* GetBaseFilename(const char* path)
{
    if (!path)
    	return "";
    
    const char* last_slash = strrchr(path, '/');
    const char* last_backslash = strrchr(path, '\\');
    const char* split = (last_slash > last_backslash) ? last_slash : last_backslash;
    
    return split ? split + 1 : path;
}





// Extracts directory path including trailing slash
static void GetDirectoryPath(const char* filepath, char* out_dir, size_t out_size)
{
    if (!filepath || !out_dir || out_size == 0)
    	return;
    
    snprintf(out_dir, out_size, "%s", filepath);
    char* last_slash = strrchr(out_dir, '/');
    char* last_backslash = strrchr(out_dir, '\\');
    char* split = (last_slash > last_backslash) ? last_slash : last_backslash;
    
    if (split)
        *(split + 1) = '\0';
    else
        out_dir[0] = '\0';
}





// Extracts filename stem without directory or extension (e.g. "models/Mdl.fbx" -> "Mdl")
static void GetFilenameStem(const char* filepath, char* out_stem, size_t out_size)
{
    if (!filepath || !out_stem || out_size == 0)
    	return;
    
    const char* base = GetBaseFilename(filepath);
    
    snprintf(out_stem, out_size, "%s", base);
    char* dot = strrchr(out_stem, '.');
    
    if (dot)
        *dot = '\0';
}















// Detects the format of the file at a specified path
ModelFormat ModelLoader_DetectFormat(const char* filepath)
{
    const char* ext = GetFileExtension(filepath);

    if (ext[0] == '\0')
        return MODEL_FORMAT_UNKNOWN;
    if (StrEqualsIgnoreCase(ext, ".obj"))
        return MODEL_FORMAT_OBJ;
    if (StrEqualsIgnoreCase(ext, ".fbx"))
        return MODEL_FORMAT_FBX;
    if (StrEqualsIgnoreCase(ext, ".stl"))
        return MODEL_FORMAT_STL;
    if (StrEqualsIgnoreCase(ext, ".gltf") || StrEqualsIgnoreCase(ext, ".glb"))
        return MODEL_FORMAT_GLTF;
    if (StrEqualsIgnoreCase(ext, ".dae"))
        return MODEL_FORMAT_DAE;
    
    return MODEL_FORMAT_UNKNOWN;
}





// Returns a config struct for the specific file format chosen
ModelFormatConfig ModelLoader_GetFormatConfig(ModelFormat format)
{
    ModelFormatConfig cfg = {0};
    cfg.format = format;

    switch (format)
    {
        case MODEL_FORMAT_OBJ:
            cfg.format_name = "Wavefront OBJ (.obj)";
            cfg.assimp_flags = aiProcess_Triangulate |
                               aiProcess_GenSmoothNormals |
                               aiProcess_CalcTangentSpace |
                               aiProcess_JoinIdenticalVertices;
            cfg.supports_skeletal_data = false;
            cfg.supports_materials = true;
            cfg.has_uvs = true;
            break;
        case MODEL_FORMAT_FBX:
            cfg.format_name = "Autodesk FBX (.fbx)";
            cfg.assimp_flags = aiProcess_Triangulate |
                               aiProcess_GenSmoothNormals |
                               aiProcess_CalcTangentSpace |
                               aiProcess_LimitBoneWeights |
                               aiProcess_PopulateArmatureData;
            cfg.supports_skeletal_data = true;
            cfg.supports_materials = true;
            cfg.has_uvs = true;
            break;
        case MODEL_FORMAT_STL:
            cfg.format_name = "Stereolithography (.stl)";
            cfg.assimp_flags = aiProcess_Triangulate |
                               aiProcess_JoinIdenticalVertices |
                               aiProcess_GenSmoothNormals;
            cfg.supports_skeletal_data = false;
            cfg.supports_materials = false;
            cfg.has_uvs = false;
            break;
        case MODEL_FORMAT_GLTF:
            cfg.format_name = "glTF / GLB (.gltf, .glb)";
            cfg.assimp_flags = aiProcess_Triangulate |
                               aiProcess_CalcTangentSpace |
                               aiProcess_LimitBoneWeights |
                               aiProcess_GenSmoothNormals;
            cfg.supports_skeletal_data = true;
            cfg.supports_materials = true;
            cfg.has_uvs = true;
            break;
        case MODEL_FORMAT_DAE:
            cfg.format_name = "Collada DAE (.dae)";
            cfg.assimp_flags = aiProcess_Triangulate |
                               aiProcess_GenSmoothNormals |
                               aiProcess_CalcTangentSpace |
                               aiProcess_LimitBoneWeights;
            cfg.supports_skeletal_data = true;
            cfg.supports_materials = true;
            cfg.has_uvs = true;
            break;
        case MODEL_FORMAT_UNKNOWN:
        default:
            cfg.format_name = "Generic / Unknown Format";
            cfg.assimp_flags = aiProcess_Triangulate |
                               aiProcess_GenSmoothNormals |
                               aiProcess_CalcTangentSpace;
            cfg.supports_skeletal_data = true;
            cfg.supports_materials = true;
            cfg.has_uvs = true;
            break;
    }

    return cfg;
}





// Gets the name of the file format as a string
const char* ModelLoader_GetFormatName(ModelFormat format)
{
    ModelFormatConfig cfg = ModelLoader_GetFormatConfig(format);
    return cfg.format_name;
}





// Returns whether a specific file format is supported
bool ModelLoader_IsFormatSupported(ModelFormat format)
{
    return format != MODEL_FORMAT_UNKNOWN;
}















// Converts an Assimp 4x4 matrix into Prism Column-Major Matrix4
static Matrix4 AssimpToColumnMatrix(struct aiMatrix4x4 m)
{
    Matrix4 m_out;

    m_out.m00 = m.a1; m_out.m01 = m.a2; m_out.m02 = m.a3; m_out.m03 = m.a4;
    m_out.m10 = m.b1; m_out.m11 = m.b2; m_out.m12 = m.b3; m_out.m13 = m.b4;
    m_out.m20 = m.c1; m_out.m21 = m.c2; m_out.m22 = m.c3; m_out.m23 = m.c4;
    m_out.m30 = m.d1; m_out.m31 = m.d2; m_out.m32 = m.d3; m_out.m33 = m.d4;
    
    return m_out;
}





// Recursively searches node tree to find accumulated transform for a mesh
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
            return true;
    }
    
    return false;
}





// Helper function to find or add a bone to a global model skeleton
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
        Log_Warning("WARNING: Max Bones exceeded (%d). Model might look distorted.", MAX_BONES);
        return 0;
    }

	// Add new bone    
    int new_id = skeleton->bone_count;
    snprintf(skeleton->bones[new_id].name, MAX_NAME_LENGTH, "%s", bone_name);
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
    snprintf(new_node->name, MAX_NAME_LENGTH, "%s", ai_node->mName.data);
    new_node->default_local_transform = AssimpToColumnMatrix(ai_node->mTransformation);
    new_node->child_count = ai_node->mNumChildren;
    
    if (new_node->child_count > 0)
    {
        new_node->children = malloc(sizeof(SkeletonNode) * new_node->child_count);
        for (uint32_t i = 0; i < new_node->child_count; i++)
            new_node->children[i] = *CopyAssimpNodeTree(ai_node->mChildren[i]);
    }
    else
    {
        new_node->children = NULL;
    }
    
    return new_node;
}















// Loads textures from an AssImp scene
static Texture* ModelLoader_ResolveAssimpTexture(AssetManager* am, const struct aiScene* scene, const struct aiMaterial* ai_mat, enum aiTextureType type, unsigned int index, const char* filepath, const char* model_name, ModelFormat format)
{
    if (!am || !scene || !ai_mat)
    	return NULL;

    struct aiString tex_path;
    if (aiGetMaterialTexture(ai_mat, type, index, &tex_path, NULL, NULL, NULL, NULL, NULL, NULL) != aiReturn_SUCCESS)
        return NULL;

    const struct aiTexture* embedded_tex = NULL;

    // glTF / GLB embedded textures start with '*' followed by integer index
    if (tex_path.data[0] == '*')
    {
        int texture_index = atoi(&tex_path.data[1]);
        if (texture_index >= 0 && texture_index < (int)scene->mNumTextures)
        {
            embedded_tex = scene->mTextures[texture_index];
        }
    }
    // FBX embedded textures (match by filename in scene->mTextures)
    else if (scene->mNumTextures > 0)
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

    if (embedded_tex != NULL)
    {
        if (embedded_tex->mHeight == 0)
        {
            char tex_name[256];
            snprintf(tex_name, sizeof(tex_name), "%s_embedded_%s_%d", model_name, tex_path.data, (int)type);
            return Asset_CreateTextureFromMemory(am, tex_name, (const unsigned char*)embedded_tex->pcData, embedded_tex->mWidth);
        }
        else
        {
            Log_Warning("WARNING: Uncompressed embedded textures are not supported for %s", tex_path.data);
            return NULL;
        }
    }

    // External File Resolution
    char model_dir[512] = {0};
    GetDirectoryPath(filepath, model_dir, sizeof(model_dir));

    char model_stem[256] = {0};
    GetFilenameStem(filepath, model_stem, sizeof(model_stem));

    const char* clean_filename = GetBaseFilename(tex_path.data);

    // Build candidate paths to test
    char candidate_paths[4][1024];
    int candidate_count = 0;

    // Candidate 1: Direct relative path from model directory
    snprintf(candidate_paths[candidate_count++], 1024, "%s%s", model_dir, tex_path.data);

    // Candidate 2: Clean filename placed alongside model
    snprintf(candidate_paths[candidate_count++], 1024, "%s%s", model_dir, clean_filename);

    // Candidate 3: FBX companion .fbm directory (<ModelStem>.fbm/<clean_filename>)
    if (format == MODEL_FORMAT_FBX && model_stem[0] != '\0')
        snprintf(candidate_paths[candidate_count++], 1024, "%s%s.fbm/%s", model_dir, model_stem, clean_filename);

    // Candidate 4: textures/ subfolder convention
    snprintf(candidate_paths[candidate_count++], 1024, "%stextures/%s", model_dir, clean_filename);

    // Try importing textures from each candidate path
    for (int i = 0; i < candidate_count; i++)
    {
        // Normalize backslashes to forward slashes
        for (char* p = candidate_paths[i]; *p; p++)
        {
            if (*p == '\\')
            	*p = '/';
        }

        // Test file existence
        FILE* f = fopen(candidate_paths[i], "rb");
        if (f)
        {
            fclose(f);
            return Asset_LoadTexture(am, clean_filename, candidate_paths[i]);
        }

        // .dds -> .png fallback check
        char* dds_ext = strstr(candidate_paths[i], ".dds");
        if (!dds_ext)
       	{
       		dds_ext = strstr(candidate_paths[i], ".DDS");
       	}
        if (dds_ext)
        {
            memcpy(dds_ext, ".png", 4);
            FILE* f_png = fopen(candidate_paths[i], "rb");
            if (f_png)
            {
                fclose(f_png);
                return Asset_LoadTexture(am, clean_filename, candidate_paths[i]);
            }
        }
    }

    Log_Warning("ASSIMP WARNING: Could not find texture '%s' for model '%s'", clean_filename, model_name);
    return NULL;
}















// Processes materials from an AssImp scene
static Material** ModelLoader_ProcessMaterials(AssetManager* am, const struct aiScene* scene, const char* filepath, const char* model_name, ModelFormat format, const ModelFormatConfig* config)
{
    // For formats with no materials (STL) or scenes with 0 materials: create a neutral default PBR material
    if (!config->supports_materials || scene->mNumMaterials == 0)
    {
        Material** mat_map = malloc(sizeof(Material*));
        char default_name[MAX_NAME_LENGTH];
        snprintf(default_name, sizeof(default_name), "%s_DefaultMat", model_name);

        Material* mat_ptr = Asset_CreateMaterial(am, default_name, Asset_GetDefaultTexture(am), NULL);
        if (mat_ptr)
        {
            mat_ptr->properties.albedo_tint = (Color){ 0.75f, 0.75f, 0.75f, 1.0f }; // Neutral light gray
            mat_ptr->properties.metallic_factor = 0.0f;
            mat_ptr->properties.roughness_factor = 0.5f;
            Asset_SyncMaterialGPU(am, mat_ptr);
        }
        mat_map[0] = mat_ptr;
        return mat_map;
    }

    // Create a temporary array to map Assmp materials to Prism Materials
    Material** material_map = malloc(sizeof(Material*) * scene->mNumMaterials);


    // ----- Loop through all materials -----
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        struct aiMaterial* ai_mat = scene->mMaterials[i];


        // 1. Albedo / Diffuse Texture
        Texture* tex_ptr = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_DIFFUSE, 0, filepath, model_name, format);
#ifdef aiTextureType_BASE_COLOR
        if (!tex_ptr)
            tex_ptr = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_BASE_COLOR, 0, filepath, model_name, format);
#else
        if (!tex_ptr)
            tex_ptr = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, (enum aiTextureType)12, 0, filepath, model_name, format);
#endif
        if (!tex_ptr)
            tex_ptr = Asset_GetDefaultTexture(am);


        // 2. Normal Map
        Texture* normal_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_NORMALS, 0, filepath, model_name, format);
        if (!normal_tex)
            normal_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_HEIGHT, 0, filepath, model_name, format);


        // 3. Metallic & Roughness Maps
        Texture* metallic_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_METALNESS, 0, filepath, model_name, format);
        if (!metallic_tex)
            metallic_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_UNKNOWN, 0, filepath, model_name, format);

        Texture* roughness_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_DIFFUSE_ROUGHNESS, 0, filepath, model_name, format);
        if (!roughness_tex)
            roughness_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_UNKNOWN, 0, filepath, model_name, format);


        // 4. Ambient Occlusion Map
        Texture* ao_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_AMBIENT_OCCLUSION, 0, filepath, model_name, format);
        if (!ao_tex)
            ao_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_AMBIENT, 0, filepath, model_name, format);
        if (!ao_tex)
            ao_tex = ModelLoader_ResolveAssimpTexture(am, scene, ai_mat, aiTextureType_LIGHTMAP, 0, filepath, model_name, format);


        // 5. Material Tint Color
        struct aiColor4D diffuse_color = {1.0f, 1.0f, 1.0f, 1.0f};
        aiGetMaterialColor(ai_mat, AI_MATKEY_COLOR_DIFFUSE, &diffuse_color);


        // 6. Build unique material name to prevent collisions across multiple models
        struct aiString ai_mat_name;
        char new_mat_name[MAX_NAME_LENGTH] = {0};
        if (aiGetMaterialString(ai_mat, AI_MATKEY_NAME, &ai_mat_name) == aiReturn_SUCCESS && ai_mat_name.length > 0)
            snprintf(new_mat_name, sizeof(new_mat_name), "%s_%s", model_name, ai_mat_name.data);
        else
            snprintf(new_mat_name, sizeof(new_mat_name), "%s_Mat_%u", model_name, i);

        Material* mat_ptr = Asset_CreateMaterial(am, new_mat_name, tex_ptr, NULL);
        if (mat_ptr)
        {
            mat_ptr->properties.albedo_tint = (Color){ diffuse_color.r, diffuse_color.g, diffuse_color.b, diffuse_color.a };

            // Format-specific physical parameters mapping
            if (format == MODEL_FORMAT_GLTF)
            {
                // Native glTF 2.0 PBR factors
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
            else if (format == MODEL_FORMAT_OBJ)
            {
                // OBJ Blinn-Phong (.mtl) to PBR approximation:
                // Roughness estimated from specular exponent Ns: roughness = sqrt(2 / (Ns + 2))
                mat_ptr->properties.metallic_factor = 0.0f; // Non-metal default

                float shininess = 0.0f;
                if (aiGetMaterialFloatArray(ai_mat, AI_MATKEY_SHININESS, &shininess, NULL) == aiReturn_SUCCESS && shininess > 0.0f)
                {
                    float r = sqrtf(2.0f / (shininess + 2.0f));
                    if (r < 0.05f) r = 0.05f;
                    if (r > 1.0f)  r = 1.0f;
                    mat_ptr->properties.roughness_factor = r;
                }
                else
                {
                    mat_ptr->properties.roughness_factor = 0.5f;
                }
            }
            else
            {
                // FBX / DAE / General fallback
                float metallic_val = 0.0f;
                if (aiGetMaterialFloatArray(ai_mat, AI_MATKEY_METALLIC_FACTOR, &metallic_val, NULL) == aiReturn_SUCCESS)
                    mat_ptr->properties.metallic_factor = metallic_val;

                float roughness_val = 0.5f;
                if (aiGetMaterialFloatArray(ai_mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness_val, NULL) == aiReturn_SUCCESS)
                {
                    mat_ptr->properties.roughness_factor = roughness_val;
                }
                else
                {
                    float shininess = 0.0f;
                    if (aiGetMaterialFloatArray(ai_mat, AI_MATKEY_SHININESS, &shininess, NULL) == aiReturn_SUCCESS && shininess > 0.0f)
                    {
                        float r = sqrtf(2.0f / (shininess + 2.0f));
                        if (r < 0.05f) r = 0.05f;
                        if (r > 1.0f) r = 1.0f;
                        mat_ptr->properties.roughness_factor = r;
                    }
                }
            }

            mat_ptr->normal_map = normal_tex;
            mat_ptr->metallic_map = metallic_tex;
            mat_ptr->roughness_map = roughness_tex;
            mat_ptr->ao_map = ao_tex;
            Asset_SyncMaterialGPU(am, mat_ptr);
        }

        material_map[i] = mat_ptr;
    }

    return material_map;
}















// Processes materials from an AssImp scene
static bool ModelLoader_ProcessMeshes(AssetManager* am, const struct aiScene* scene, Model* new_model, Material** material_map, const ModelFormatConfig* config, const char* model_name)
{
    bool has_animations = (config->supports_skeletal_data && scene->mNumAnimations > 0);

    for (uint32_t m = 0; m < scene->mNumMeshes; m++)
    {
        struct aiMesh* ai_mesh = scene->mMeshes[m];
        uint32_t vertex_count = ai_mesh->mNumVertices;
        uint32_t max_index_count = ai_mesh->mNumFaces * 3;
        uint32_t* indices = malloc(sizeof(uint32_t) * max_index_count);

        Matrix4 node_transform = Matrix4Identity();
        const char* real_node_name = "UnknownNode";
        GetMeshGlobalTransform(scene->mRootNode, m, Matrix4Identity(), &node_transform, &real_node_name);

        bool is_skinned = (config->supports_skeletal_data && ai_mesh->mNumBones > 0);
        new_model->nodes[m].is_skinned = is_skinned;

        // Material assignment
        if (config->supports_materials && scene->mNumMaterials > 0 && ai_mesh->mMaterialIndex < scene->mNumMaterials)
            new_model->nodes[m].material = material_map[ai_mesh->mMaterialIndex];
        else
            new_model->nodes[m].material = material_map[0];

        // ---------------- Skinned Mesh Branch ----------------
        if (is_skinned)
        {
            SkinnedMesh* sub_mesh = Asset_AllocateSkinnedMesh(am);
            if (!sub_mesh)
            {
                Log_Error("ERROR: Skinned mesh cache limit reached for '%s'.", model_name);
                free(indices);
                continue;
            }

            Vertex3DSkinned* vertices = malloc(sizeof(Vertex3DSkinned) * vertex_count);

            // Extract Vertices
            for (uint32_t i = 0; i < vertex_count; i++)
            {
                Vertex3DSkinned v;
                v.position = (Vector3){ ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };

                if (ai_mesh->mNormals)
                    v.normal = (Vector3){ ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };
                else
                    v.normal = (Vector3){ 0.0f, 1.0f, 0.0f };

                if (ai_mesh->mTextureCoords[0])
                    v.uv = (Vector2){ ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
                else
                    v.uv = (Vector2){ 0.0f, 0.0f };

                if (ai_mesh->mTangents)
                    v.tangent = (Vector3){ ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z };
                else
                {
                    Vector3 n = v.normal;
                    Vector3 c = (fabsf(n.z) < 0.99f) ? (Vector3){ -n.y, n.x, 0.0f } : (Vector3){ 0.0f, -n.z, n.y };
                    float l = sqrtf(c.x*c.x + c.y*c.y + c.z*c.z);
                    v.tangent = (l > 0.0001f) ? (Vector3){ c.x/l, c.y/l, c.z/l } : (Vector3){ 1.0f, 0.0f, 0.0f };
                }

                for (int b = 0; b < MAX_BONE_INFLUENCE; b++)
                {
                    v.bone_ids[b] = -1;
                    v.bone_weights[b] = 0.0f;
                }
                vertices[i] = v;
            }

            // Extract Bone Weights
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
                        if (vertices[vertex_id].bone_ids[slot] == -1)
                        {
                            vertices[vertex_id].bone_ids[slot] = bone_id;
                            vertices[vertex_id].bone_weights[slot] = weight;
                            break;
                        }
                    }
                }
            }

            // Extract Indices
            uint32_t actual_index_count = 0;
            for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++)
            {
                struct aiFace face = ai_mesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++)
                    indices[actual_index_count++] = face.mIndices[j];
            }

            // Send to GPU and cache
            MeshHandle mesh_handle = Render_CreateSkinnedMesh(am->renderer, vertices, vertex_count, indices, actual_index_count);
            if (real_node_name[0] == '\0')
                snprintf(sub_mesh->name, sizeof(sub_mesh->name), "%s_Mesh_%d", model_name, m);
            else
                snprintf(sub_mesh->name, sizeof(sub_mesh->name), "%s", real_node_name);

            sub_mesh->gpu_handle = mesh_handle;
            sub_mesh->vertices = vertices;
            sub_mesh->vertex_count = vertex_count;
            sub_mesh->indices = indices;
            sub_mesh->index_count = actual_index_count;
            sub_mesh->local_bounds = SkinnedMesh_CalculateAABB(vertices, vertex_count);

            // TODO: Make animations change the local bounds of AABB's
            // Inflate bounds for animations to prevent premature culling
            sub_mesh->local_bounds = AABB_InflateFromCenter(sub_mesh->local_bounds, 2.0f);

            new_model->nodes[m].skinned_mesh = sub_mesh;
            new_model->nodes[m].mesh = NULL;
        }

        // ---------------- Static Mesh Branch ----------------
        else
        {
            Mesh* sub_mesh = Asset_AllocateMesh(am);
            if (!sub_mesh)
            {
                Log_Error("ERROR: Static mesh cache limit reached for '%s'.", model_name);
                free(indices);
                continue;
            }

            Vertex3D* vertices = malloc(sizeof(Vertex3D) * vertex_count);

            // Extract Vertices
            for (uint32_t i = 0; i < vertex_count; i++)
            {
                Vertex3D v;
                Vector3 raw_pos = (Vector3){ ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
                Vector3 raw_norm = ai_mesh->mNormals ? (Vector3){ ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z } : (Vector3){ 0.0f, 1.0f, 0.0f };
                Vector3 raw_tan = ai_mesh->mTangents ? (Vector3){ ai_mesh->mTangents[i].x, ai_mesh->mTangents[i].y, ai_mesh->mTangents[i].z } : (Vector3){ 0.0f, 0.0f, 0.0f };

                // Apply node transformation directly to rigid geometry
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
                    	v.normal.x /= length;
                    	v.normal.y /= length;
                    	v.normal.z /= length;
                    }

                    float tan_length = sqrtf(v.tangent.x*v.tangent.x + v.tangent.y*v.tangent.y + v.tangent.z*v.tangent.z);
                    if (tan_length > 0.0001f)
                    {
                    	v.tangent.x /= tan_length;
                    	v.tangent.y /= tan_length;
                    	v.tangent.z /= tan_length;
                    }
                }
                else
                {
                    v.position = raw_pos;
                    v.normal = raw_norm;
                    v.tangent = raw_tan;
                }

                // UVs (fallback to 0,0 for STL or untextured models)
                if (ai_mesh->mTextureCoords[0])
                    v.uv = (Vector2){ ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
                else
                    v.uv = (Vector2){ 0.0f, 0.0f };

                // Tangent fallback
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

            // Extract Indices
            uint32_t actual_index_count = 0;
            for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++)
            {
                struct aiFace face = ai_mesh->mFaces[i];
                for (uint32_t j = 0; j < face.mNumIndices; j++)
                    indices[actual_index_count++] = face.mIndices[j];
            }

            // Send to GPU and cache
            MeshHandle mesh_handle = Render_CreateStaticMesh(am->renderer, vertices, vertex_count, indices, actual_index_count);
            if (real_node_name[0] == '\0')
                snprintf(sub_mesh->name, sizeof(sub_mesh->name), "%s_Mesh_%d", model_name, m);
            else
                snprintf(sub_mesh->name, sizeof(sub_mesh->name), "%s", real_node_name);

            sub_mesh->gpu_handle = mesh_handle;
            sub_mesh->vertices = vertices;
            sub_mesh->vertex_count = vertex_count;
            sub_mesh->indices = indices;
            sub_mesh->index_count = actual_index_count;
            sub_mesh->local_bounds = Mesh_CalculateAABB(vertices, vertex_count);

            new_model->nodes[m].mesh = sub_mesh;
            new_model->nodes[m].skinned_mesh = NULL;
        }
    }

    return true;
}















// Processes animations from a AssImp scene, for a model
static void ModelLoader_ProcessAnimations(const struct aiScene* scene, Model* new_model)
{
    new_model->animation_count = scene->mNumAnimations;
    if (new_model->animation_count == 0)
    {
        new_model->animations = NULL;
        return;
    }

    // Allocate array of pointers
    new_model->animations = malloc(sizeof(AnimationClip*) * new_model->animation_count);

    // ----- Loop through all animations -----
    for (uint32_t a = 0; a < scene->mNumAnimations; a++)
    {
        struct aiAnimation* ai_anim = scene->mAnimations[a];
        AnimationClip* clip = malloc(sizeof(AnimationClip));

        snprintf(clip->name, sizeof(clip->name), "%s", ai_anim->mName.data);
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

            snprintf(channel->bone_name, sizeof(channel->bone_name), "%s", ai_channel->mNodeName.data);

            // Positions
            channel->position_count = ai_channel->mNumPositionKeys;
            channel->positions = malloc(sizeof(VectorKey) * channel->position_count);
            for (uint32_t k = 0; k < channel->position_count; k++)
            {
                channel->positions[k].time = (float)ai_channel->mPositionKeys[k].mTime;
                channel->positions[k].value = (Vector3){
                    ai_channel->mPositionKeys[k].mValue.x,
                    ai_channel->mPositionKeys[k].mValue.y,
                    ai_channel->mPositionKeys[k].mValue.z
                };
            }

            // Rotations (Assimp Quaternions are W, X, Y, Z order)
            channel->rotation_count = ai_channel->mNumRotationKeys;
            channel->rotations = malloc(sizeof(QuaternionKey) * channel->rotation_count);
            for (uint32_t k = 0; k < channel->rotation_count; k++)
            {
                channel->rotations[k].time = (float)ai_channel->mRotationKeys[k].mTime;
                channel->rotations[k].value = (Quaternion){
                    ai_channel->mRotationKeys[k].mValue.x,
                    ai_channel->mRotationKeys[k].mValue.y,
                    ai_channel->mRotationKeys[k].mValue.z,
                    ai_channel->mRotationKeys[k].mValue.w
                };
            }

            // Scales
            channel->scale_count = ai_channel->mNumScalingKeys;
            channel->scales = malloc(sizeof(VectorKey) * channel->scale_count);
            for (uint32_t k = 0; k < channel->scale_count; k++)
            {
                channel->scales[k].time = (float)ai_channel->mScalingKeys[k].mTime;
                channel->scales[k].value = (Vector3){
                    ai_channel->mScalingKeys[k].mValue.x,
                    ai_channel->mScalingKeys[k].mValue.y,
                    ai_channel->mScalingKeys[k].mValue.z
                };
            }
        }

        new_model->animations[a] = clip;
    }
}















// Loads a model from the specified file path
Model* ModelLoader_Load(AssetManager* am, const char* name, const char* filepath)
{
    if (!am || !filepath || !name)
        return NULL;

    ModelFormat format = ModelLoader_DetectFormat(filepath);
    ModelFormatConfig config = ModelLoader_GetFormatConfig(format);

    if (format == MODEL_FORMAT_UNKNOWN)
        Log_Warning("MODEL LOADER: Unrecognized or unsupported file extension for '%s'. Attempting default Assimp import.", filepath);
    else
        Log_Info("MODEL LOADER: Loading '%s' [%s] from '%s'", name, config.format_name, filepath);


    // Ingest file using format specific Assimp post-processing flags
    const struct aiScene* scene = aiImportFile(filepath, config.assimp_flags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        Log_Error("ASSIMP ERROR: %s", aiGetErrorString());
        return NULL;
    }

    Model* new_model = malloc(sizeof(Model));
    if (!new_model)
    {
        Log_Error("ERROR: Out of memory allocating Model container for '%s'", name);
        aiReleaseImport(scene);
        return NULL;
    }
    memset(new_model, 0, sizeof(Model));

    snprintf(new_model->name, sizeof(new_model->name), "%s", name);
    new_model->node_count = scene->mNumMeshes;
    new_model->nodes = malloc(sizeof(ModelNode) * new_model->node_count);
    memset(new_model->nodes, 0, sizeof(ModelNode) * new_model->node_count);

    // Initialize skeleton container if format supports skinning
    if (config.supports_skeletal_data)
    {
        new_model->skeleton = malloc(sizeof(Skeleton));
        memset(new_model->skeleton, 0, sizeof(Skeleton));
    }
    else
    {
        new_model->skeleton = NULL;
    }

    // Process materials using format-specific physical mapping
    Material** material_map = ModelLoader_ProcessMaterials(am, scene, filepath, name, format, &config);

    // Process mesh geometry with resilient fallbacks
    ModelLoader_ProcessMeshes(am, scene, new_model, material_map, &config, name);

    // Finalize skeleton node hierarchy
    if (new_model->skeleton != NULL)
    {
        if (new_model->skeleton->bone_count > 0)
        {
            new_model->skeleton->root_node = CopyAssimpNodeTree(scene->mRootNode);
        }
        else
        {
            // Format supports skeleton, but this specific file contains no bones
            free(new_model->skeleton);
            new_model->skeleton = NULL;
        }
    }

    // Process animations if supported
    if (config.supports_skeletal_data && scene->mNumAnimations > 0)
    {
        ModelLoader_ProcessAnimations(scene, new_model);
    }
    else
    {
        new_model->animations = NULL;
        new_model->animation_count = 0;
    }

    free(material_map);
    aiReleaseImport(scene);

    return new_model;
}