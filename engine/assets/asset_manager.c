#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "model_loader.h"

#include "asset_manager.h"
#include "core/io_core.h"
#include "core/mesh_core.h"
#include "core/image_core.h"
#include "core/log_core.h"
#include "core/font_core.h"





// Forward declaration for cleanup helper
static void FreeSkeletonNodeChildren(SkeletonNode* node);





// Initializes an empty asset manager
AssetManager* Asset_Init(Renderer* r)
{
    if (!r)
        return NULL;

    // Allocate and initialize instance
    AssetManager* am = calloc(1, sizeof(AssetManager));
    if (!am)
        return NULL;

    am->renderer = r;
    
    // Initialize all counts to 0
    am->shader_count = 0;
    am->texture_count = 0;
    am->mesh_count = 0;
    am->skinned_mesh_count = 0;
    am->model_count = 0;
    am->font_count = 0;
    am->material_count = 0;
    am->env_map_count = 0;
    memset(am->material_gpu_desc_valid, 0, sizeof(am->material_gpu_desc_valid));

    // Pre-initialize default assets
    Asset_GetBuiltinQuad(am);
    Asset_GetBuiltinCube(am);
    Asset_GetBuiltinSphere(am);
    Asset_GetDefaultTexture(am);

    return am;
}










// Frees all allocations owned by an asset manager instance
void Asset_Shutdown(AssetManager* am)
{
    if (!am)
        return;

    // Free all models and their nodes
    for (uint32_t i = 0; i < am->model_count; i++)
    {
        Model* model = am->model_cache[i];
        if (!model)
            continue;

        if (model->nodes)
            free(model->nodes);
        
        // Free all animations
        if (model->animations)
        {
            for (uint32_t a = 0; a < model->animation_count; a++)
            {
                AnimationClip* clip = model->animations[a];
                if (!clip)
                    continue;

                if (clip->channels)
                {
                    for (uint32_t c = 0; c < clip->channel_count; c++)
                    {
                        free(clip->channels[c].positions);
                        free(clip->channels[c].rotations);
                        free(clip->channels[c].scales);
                    }
                    free(clip->channels);
                }
                free(clip);
            }
            free(model->animations);
        }

        // Free all skeletons
        if (model->skeleton)
        {
            if (model->skeleton->root_node)
            {
                FreeSkeletonNodeChildren(model->skeleton->root_node);
                free(model->skeleton->root_node);
            }
            free(model->skeleton);
        }
        
        free(model);
        am->model_cache[i] = NULL;
    }

    // Free all meshes
    for (uint32_t i = 0; i < am->mesh_count; i++)
    {
        if (am->mesh_cache[i].vertices)
        {
            free(am->mesh_cache[i].vertices);
            am->mesh_cache[i].vertices = NULL;
        }

        if (am->mesh_cache[i].indices)
        {
            free(am->mesh_cache[i].indices);
            am->mesh_cache[i].indices = NULL;
        }
    }

    // Free all skinned meshes
    for (uint32_t i = 0; i < am->skinned_mesh_count; i++)
    {
        if (am->skinned_mesh_cache[i].vertices)
        {
            free(am->skinned_mesh_cache[i].vertices);
            am->skinned_mesh_cache[i].vertices = NULL;
        }

        if (am->skinned_mesh_cache[i].indices)
        {
            free(am->skinned_mesh_cache[i].indices);
            am->skinned_mesh_cache[i].indices = NULL;
        }
    }

    free(am);
}




















// Finds a free slot in the cached models array
static int FindFreeModelSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->model_count; i++)
    {
        if (am->model_cache[i] == NULL)
            return i;
    }

    if (am->model_count < MAX_CACHED_MODELS)
        return am->model_count++;
    
    return -1;
}





// Finds a free slot in the cached textures array
static Texture* FindFreeTextureSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (am->texture_cache[i].name[0] == '\0')
        {
            am->texture_cache[i].id = i;
            return &am->texture_cache[i];
        }
    }

    if (am->texture_count < MAX_CACHED_TEXTURES)
    {
        Texture* t = &am->texture_cache[am->texture_count];
        t->id = am->texture_count++;
        return t;
    }
    
    return NULL;
}





// Finds a free slot in the cached shaders array
static Shader* FindFreeShaderSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->shader_count; i++)
    {
        if (am->shader_cache[i].name[0] == '\0')
        {
            am->shader_cache[i].id = i;
            return &am->shader_cache[i];
        }
    }

    if (am->shader_count < MAX_CACHED_SHADERS)
    {
        Shader* s = &am->shader_cache[am->shader_count];
        s->id = am->shader_count++;
        return s;
    }
    
    return NULL;
}





// Finds a free slot in the cached meshes array
static Mesh* FindFreeMeshSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->mesh_count; i++)
    {
        if (am->mesh_cache[i].name[0] == '\0')
        {
            am->mesh_cache[i].id = i;
            return &am->mesh_cache[i];
        }
    }

    if (am->mesh_count < MAX_CACHED_MESHES)
    {
        Mesh* m = &am->mesh_cache[am->mesh_count];
        m->id = am->mesh_count++;
        return m;
    }

    return NULL;
}





// Finds a free slot in the cached skinned meshes array
static SkinnedMesh* FindFreeSkinnedMeshSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->skinned_mesh_count; i++)
    {
        if (am->skinned_mesh_cache[i].name[0] == '\0')
        {
            am->skinned_mesh_cache[i].id = i;
            return &am->skinned_mesh_cache[i];
        }
    }

    if (am->skinned_mesh_count < MAX_CACHED_SKINNED_MESHES)
    {
        SkinnedMesh* m = &am->skinned_mesh_cache[am->skinned_mesh_count];
        m->id = am->skinned_mesh_count++;
        return m;
    }
    
    return NULL;
}




// Allocates a slot in the cached meshes array
Mesh* Asset_AllocateMesh(AssetManager* am)
{
    return FindFreeMeshSlot(am);
}

// Allocates a slot in the cached skinned meshes array
SkinnedMesh* Asset_AllocateSkinnedMesh(AssetManager* am)
{
    return FindFreeSkinnedMeshSlot(am);
}





// Finds a free slot in the cached environment maps array
static EnvironmentMap* FindFreeEnvMapSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->env_map_count; i++)
    {
        if (am->env_map_cache[i].name[0] == '\0')
        {
            am->env_map_cache[i].id = i;
            return &am->env_map_cache[i];
        }
    }

    if (am->env_map_count < MAX_CACHED_TEXTURES)
    {
        EnvironmentMap* m = &am->env_map_cache[am->env_map_count];
        m->id = am->env_map_count++;
        return m;
    }
    
    return NULL;
}





// Finds a free slot in the cached fonts array
static Font* FindFreeFontSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->font_count; i++)
    {
        if (am->font_cache[i].name[0] == '\0')
            return &am->font_cache[i];
    }

    if (am->font_count < MAX_CACHED_FONTS)
        return &am->font_cache[am->font_count++];

    return NULL;
}





// Finds a free slot in the cached materials array
static Material* FindFreeMaterialSlot(AssetManager* am)
{
    for (uint32_t i = 0; i < am->material_count; i++)
    {
        if (!am->material_cache[i].active)
        {
            am->material_cache[i].id = i;
            return &am->material_cache[i];
        }
    }

    if (am->material_count < MAX_MATERIALS)
    {
        Material* m = &am->material_cache[am->material_count];
        m->id = am->material_count++;
        return m;
    }
    
    return NULL;
}




















// Main model loader function
Model* Asset_LoadModel(AssetManager* am, const char* name, const char* filepath)
{
    if (!am || !name || !filepath)
        return NULL;

    Model* new_model = ModelLoader_Load(am, name, filepath);
    if (!new_model)
        return NULL;

    int slot = FindFreeModelSlot(am);
    if (slot != -1)
        am->model_cache[slot] = new_model;
    else
        Log_Warning("WARNING: MAX_CACHED_MODELS limit (%d) reached", MAX_CACHED_MODELS);

    return new_model;
}










// Loads a texture from a filepath
Texture* Asset_LoadTexture(AssetManager* am, const char* name, const char* filepath)
{
    if (!am)
        return NULL;

    // Check if the texture is already loaded
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (strcmp(am->texture_cache[i].name, name) == 0)
        {
            return &am->texture_cache[i];
        }
    }

    // Load pixels
    ImageData img = Image_Load(filepath, true);
    if (!img.pixels)
    {
        Log_Error("ERROR: Failed to load texture: %s\n", filepath);
        return NULL;
    }

    // Create mesh on GPU
    TextureHandle handle = Render_CreateTexture2D(am->renderer, img.pixels, img.width, img.height, img.channels);

    // Free image from memory
    Image_Free(&img);

    Texture* new_text = FindFreeTextureSlot(am);
    if (new_text)
    {
        snprintf(new_text->name, sizeof(new_text->name), "%s", name);
        new_text->gpu_handle = handle;
        new_text->width = img.width;
        new_text->height = img.height;
        new_text->channels = img.channels;
    }
    else
    {
        Log_Error("ERROR: Asset Manager out of texture cache space.\n");
        Render_DestroyTexture(am->renderer, handle);
    }

    return new_text;
}










// Loads a Cube Map texture from 6 separate images
Texture* Asset_LoadCubemapTexture(AssetManager* am, const char* name, const char* right, const char* left, const char* top, const char* bottom, const char* front, const char* back)
{
    if (!am)
        return NULL;

    // Check if it's already loaded
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (strcmp(am->texture_cache[i].name, name) == 0)
            return &am->texture_cache[i];
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
            Log_Error("ERROR: Failed to load Cube Map face: %s\n", paths[i]);
            failed = true;
            break; // Stop loading if one fails
        }
    }

    // If any image failed, clean up the successful ones and abort
    if (failed)
    {
        for (int i = 0; i < 6; i++)
        {
            if (images[i].pixels)
                Image_Free(&images[i]);
        }

        return NULL;
    }

    // Send all 6 pixel arrays to the GPU (Assuming all 6 faces of a cube map have the exact same width/height/channels)
    TextureHandle handle = Render_CreateCubemap(
        am->renderer,
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
    Texture* new_text = FindFreeTextureSlot(am);
    if (new_text)
    {
        snprintf(new_text->name, sizeof(new_text->name), "%s", name);
        new_text->gpu_handle = handle;
        new_text->width = images[0].width;
        new_text->height = images[0].height;
        new_text->channels = images[0].channels;        
    }
    else
    {
        Log_Error("ERROR: Asset Manager out of texture cache space.\n");
        Render_DestroyTexture(am->renderer, handle);
    }

    return new_text;
}










// Loads an environment map from an HDR file
EnvironmentMap* Asset_LoadEnvironmentMap(AssetManager* am, const char* filepath)
{
    if (!am)
        return NULL;

    // Check if it's already loaded
    for (uint32_t i = 0; i < am->env_map_count; i++)
    {
        if (strcmp(am->env_map_cache[i].name, filepath) == 0)
            return &am->env_map_cache[i];
    }

    ImageDataFloat img = Image_LoadFloat(filepath, true);
    if (!img.pixels)
    {
        Log_Error("ERROR: Failed to load HDR environment map: %s", filepath);
        return NULL;
    }

    RenderEnvironmentMapDesc env_desc = { img.pixels, img.width, img.height };
    EnvironmentMapHandle handle = Render_CreateEnvironmentMap(am->renderer, &env_desc);
    Image_FreeFloat(&img);
    if (handle.id == 0)
        return NULL;

    EnvironmentMap* m = FindFreeEnvMapSlot(am);
    if (m)
    {
        memset(m, 0, sizeof(*m));
        snprintf(m->name, sizeof(m->name), "%s", filepath);
        m->gpu_handle = handle;

        return m;
    }

    Render_DestroyEnvironmentMap(am->renderer, handle);
    return NULL;
}










// Creates an environment map (without IBL features) from 6 standard images
EnvironmentMap* Asset_LoadEnvironmentMapFromSkybox(AssetManager* am, const char* name, const char* right, const char* left, const char* top, const char* bottom, const char* front, const char* back)
{
    if (!am)
        return NULL;

    // Check if it's already loaded
    for (uint32_t i = 0; i < am->env_map_count; i++)
    {
        if (strcmp(am->env_map_cache[i].name, name) == 0)
            return &am->env_map_cache[i];
    }

    Texture* skybox_tex = Asset_LoadCubemapTexture(am, name, right, left, top, bottom, front, back);
    if (!skybox_tex)
        return NULL;

    RenderEnvironmentMapDesc desc = {0};
    desc.skybox = skybox_tex->gpu_handle;
    EnvironmentMapHandle handle = Render_CreateEnvironmentMap(am->renderer, &desc);
    if (handle.id == 0)
        return NULL;

    EnvironmentMap* m = FindFreeEnvMapSlot(am);
    if (m)
    {
        memset(m, 0, sizeof(*m));
        snprintf(m->name, sizeof(m->name), "%s", name);
        m->gpu_handle = handle;

        return m;
    }

    Render_DestroyEnvironmentMap(am->renderer, handle);
    return NULL;
}










// Loads a Font from disk
static Font* Asset_LoadFontInternal(AssetManager* am, const char* name, const char* filepath, float pixel_height, bool sdf)
{
    if (!am || !name || !filepath)
        return NULL;

    for (uint32_t i = 0; i < am->font_count; i++)
    {
        if (strcmp(am->font_cache[i].name, name) == 0)
            return &am->font_cache[i];
    }

    Font* font = FindFreeFontSlot(am);
    if (!font)
    {
        Log_Error("ERROR: Font cache is full");
        return NULL;
    }

    Texture* atlas_tex = FindFreeTextureSlot(am);
    if (!atlas_tex)
    {
        Log_Error("ERROR: Texture cache is full while loading font");
        return NULL;
    }

    ImageData atlas = {0};
    Font baked = {0};
    bool baked_ok = false;
    if (sdf)
        baked_ok = Font_BakeSDFFromFile(&baked, filepath, pixel_height, &atlas);
    else
        baked_ok = Font_BakeBitmapFromFile(&baked, filepath, pixel_height, &atlas);

    if (!baked_ok)
        return NULL;

    TextureHandle handle = Render_CreateTexture2D(am->renderer, atlas.pixels, atlas.width, atlas.height, atlas.channels);
    Image_Free(&atlas);

    snprintf(atlas_tex->name, sizeof(atlas_tex->name), "%s", name);
    atlas_tex->gpu_handle = handle;
    atlas_tex->width = baked.atlas_width;
    atlas_tex->height = baked.atlas_height;
    atlas_tex->channels = 4;

    *font = baked;
    snprintf(font->name, sizeof(font->name), "%s", name);
    font->texture_atlas = atlas_tex;

    return font;
}





// Loads a Font from disk as signed distance fields
Font* Asset_LoadFont(AssetManager* am, const char* name, const char* filepath, float pixel_height)
{
    return Asset_LoadFontInternal(am, name, filepath, pixel_height, true);
}





// Loads a Font from disk as a plain bitmap atlas, which is only pixel-exact when drawn at pixel_height
Font* Asset_LoadFontBitmap(AssetManager* am, const char* name, const char* filepath, float pixel_height)
{
    return Asset_LoadFontInternal(am, name, filepath, pixel_height, false);
}










// Loads a vertex and fragment shader from a file path and compiles it to a full shader
Shader* Asset_LoadShader(AssetManager* am, const char* name, const char* vert_path, const char* frag_path)
{
    if (!am)
        return NULL;

    // Check if we already loaded this exact shader
    for (uint32_t i = 0; i < am->shader_count; i++)
    {
        if (strcmp(am->shader_cache[i].name, name) == 0)
        {
            return &am->shader_cache[i];
        }
    }

    // Read shaders from file
    char* v_src = IO_ReadTextFile(vert_path);
    char* f_src = IO_ReadTextFile(frag_path);

    if (!v_src || !f_src)
    {
        Log_Error("ERROR: Failed to read shader files for '%s'\n", name);
        if (v_src) free(v_src);
        if (f_src) free(f_src);
        return NULL;
    }

    Shader* new_shad = FindFreeShaderSlot(am);
    if (!new_shad)
    {
        Log_Error("ERROR: Asset Manager out of shader cache space.\n");
        if (v_src) free(v_src);
        if (f_src) free(f_src);
        return NULL;
    }

    // Compile on GPU
    ShaderHandle new_handle = Render_CreateShaderGLSL(am->renderer, v_src, f_src);

    // Free shaders from memory
    free(v_src);
    free(f_src);

    snprintf(new_shad->name, sizeof(new_shad->name), "%s", name);
    new_shad->gpu_handle = new_handle;

    return new_shad;
}




















// Recursively frees a skeleton from the root node
static void FreeSkeletonNodeChildren(SkeletonNode* node)
{
    if (!node || !node->children)
        return;
    
    for (uint32_t i = 0; i < node->child_count; i++)
        FreeSkeletonNodeChildren(&node->children[i]);
    
    free(node->children);
}










// Unloads a model from memory
void Asset_UnloadModel(AssetManager* am, Model* model)
{
    if (!am || !model)
        return;
    
    // Unload all nodes (meshes)
    if (model->nodes)
    {
        for (uint32_t i = 0; i < model->node_count; i++)
        {
            if (model->nodes[i].is_skinned && model->nodes[i].skinned_mesh)
                Asset_UnloadSkinnedMesh(am, model->nodes[i].skinned_mesh);
            else if (!model->nodes[i].is_skinned && model->nodes[i].mesh)
                Asset_UnloadMesh(am, model->nodes[i].mesh);
        }

        free(model->nodes);
    }
    
    // Unload animations
    if (model->animations)
    {
        for (uint32_t i = 0; i < model->animation_count; i++)
        {
            if (model->animations[i]->channels)
                free(model->animations[i]->channels);
            
            free(model->animations[i]);
        }

        free(model->animations);
    }
    
    // Free skeleton
    if (model->skeleton)
    {
        if (model->skeleton->root_node)
        {
            FreeSkeletonNodeChildren(model->skeleton->root_node);
            free(model->skeleton->root_node);
        }

        free(model->skeleton);
    }
    
    // Free from cache
    for (uint32_t i = 0; i < am->model_count; i++)
    {
        if (am->model_cache[i] == model)
        {
            am->model_cache[i] = NULL;
            break;
        }
    }
    
    free(model);
}










// Unloads a mesh from memory
void Asset_UnloadMesh(AssetManager* am, Mesh* mesh)
{
    if (!am || !mesh || mesh->name[0] == '\0')
        return;
    
    Render_DestroyMesh(am->renderer, mesh->gpu_handle);
    
    if (mesh->vertices) { free(mesh->vertices); mesh->vertices = NULL; }
    if (mesh->indices) { free(mesh->indices); mesh->indices = NULL; }
    
    mesh->name[0] = '\0';
}










// Unloads a mesh from memory
void Asset_UnloadSkinnedMesh(AssetManager* am, SkinnedMesh* mesh)
{
    if (!am || !mesh || mesh->name[0] == '\0')
        return;
    
    Render_DestroyMesh(am->renderer, mesh->gpu_handle);
    
    if (mesh->vertices) { free(mesh->vertices); mesh->vertices = NULL; }
    if (mesh->indices) { free(mesh->indices); mesh->indices = NULL; }
    
    mesh->name[0] = '\0';
}










// Unloads a texture from memory
void Asset_UnloadTexture(AssetManager* am, Texture* texture)
{
    if (!am || !texture || texture->name[0] == '\0')
        return;
    
    Render_DestroyTexture(am->renderer, texture->gpu_handle);
    texture->name[0] = '\0';
}










// Unloads environment map from memory
void Asset_UnloadEnvironmentMap(AssetManager* am, EnvironmentMap* env_map)
{
    if (!am || !env_map || env_map->name[0] == '\0')
        return;
    
    Render_DestroyEnvironmentMap(am->renderer, env_map->gpu_handle);
    env_map->name[0] = '\0';
}










// Unloads a font from memory
void Asset_UnloadFont(AssetManager* am, Font* font)
{
    if (!am || !font || font->name[0] == '\0')
        return;
    
    if (font->texture_atlas)
        Asset_UnloadTexture(am, font->texture_atlas);
    
    font->name[0] = '\0';
}










// Unloads a shader from memory
void Asset_UnloadShader(AssetManager* am, Shader* shader)
{
    if (!am || !shader || shader->name[0] == '\0')
        return;
    
    Render_DestroyShader(am->renderer, shader->gpu_handle);
    shader->name[0] = '\0';
}










// Unloads a material from memory
void Asset_UnloadMaterial(AssetManager* am, Material* mat)
{
    if (!am || !mat || !mat->active)
        return;

    Render_DestroyMaterial(am->renderer, mat->gpu_handle);
    
    mat->active = false;
    am->material_gpu_desc_valid[mat->id] = false;
}




















// Generate built in quad mesh (basic square)
Mesh* Asset_GetBuiltinQuad(AssetManager* am)
{
    if (!am)
        return NULL;

    // If we already made the quad, return the ID
    if (am->builtin_quad != NULL)
        return am->builtin_quad;

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

    MeshHandle gpu_handle = Render_CreateStaticMesh(am->renderer, heap_vertices, 4, heap_indices, 6);

    // Cache the mesh
    Mesh* m = FindFreeMeshSlot(am);
    if (m)
    {
        snprintf(m->name, sizeof(m->name), "Default Quad");
        m->gpu_handle = gpu_handle;

        m->vertices = heap_vertices;
        m->indices = heap_indices;
        m->vertex_count = 4;
        m->index_count = 6;
        m->local_bounds = Mesh_CalculateAABB(heap_vertices, 4);

        am->builtin_quad = m;
    }
    else
    {
        free(heap_vertices);
        free(heap_indices);
    }

    return am->builtin_quad;
}










// Generate a standard cube mesh
Mesh* Asset_GetBuiltinCube(AssetManager* am)
{
    if (!am)
        return NULL;

    if (am->builtin_cube != NULL)
        return am->builtin_cube;


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


    MeshHandle gpu_handle = Render_CreateStaticMesh(am->renderer, heap_vertices, 24, heap_indices, 36);
    
    // Cache the mesh
    Mesh* m = FindFreeMeshSlot(am);
    if (m)
    {
        snprintf(m->name, sizeof(m->name), "Default Cube");
        m->gpu_handle = gpu_handle;

        m->vertices = heap_vertices;
        m->indices = heap_indices;
        m->vertex_count = 24;
        m->index_count = 36;
        m->local_bounds = Mesh_CalculateAABB(heap_vertices, 24);

        am->builtin_cube = m;
    }
    else
    {
        free(heap_vertices);
        free(heap_indices);
    }

    return am->builtin_cube;
}










// Create a standard sphere
Mesh* Asset_GetBuiltinSphere(AssetManager* am)
{
    if (!am)
        return NULL;

    if (am->builtin_sphere != NULL)
        return am->builtin_sphere;

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
    MeshHandle gpu_handle = Render_CreateStaticMesh(am->renderer, vertices, vertex_count, indices, index_count);

    Mesh* m = FindFreeMeshSlot(am);
    if (m)
    {
        snprintf(m->name, sizeof(m->name), "Default Sphere");
        m->gpu_handle = gpu_handle;

        m->vertices = vertices;
        m->indices = indices;
        m->vertex_count = vertex_count;
        m->index_count = index_count;
        m->local_bounds = Mesh_CalculateAABB(vertices, vertex_count);

        am->builtin_sphere = m;
    }
    else
    {
        free(vertices);
        free(indices);
    }

    return am->builtin_sphere;
}











// Create the default texture (all white)
Texture* Asset_GetDefaultTexture(AssetManager* am)
{
    if (!am)
        return NULL;

    // If already made, return the handle
    if (am->default_texture != NULL)
        return am->default_texture;

    TextureHandle handle = (TextureHandle){ 1 };

    if (am->texture_count < MAX_CACHED_TEXTURES)
    {
        Texture* t = &am->texture_cache[am->texture_count];
        snprintf(t->name, sizeof(t->name), "Default Texture");
        t->id = am->texture_count;
        t->gpu_handle = handle;
        t->width = 1;
        t->height = 1;
        t->channels = 4;

        am->default_texture = t;

        am->texture_count++;
    }


    return am->default_texture;
}




















// Look up a model by its string name
Model* Asset_GetModelByName(AssetManager* am, const char* name)
{
    if (!am || !name)
        return NULL;

    for (uint32_t i = 0; i < am->model_count; i++)
    {
        if (am->model_cache[i] != NULL && strcmp(am->model_cache[i]->name, name) == 0)
            return am->model_cache[i];
    }

    return NULL;
}





// Look up a mesh by its string name
Mesh* Asset_GetMeshByName(AssetManager* am, const char* name)
{
    if (!am || !name)
        return NULL;

    for (uint32_t i = 0; i < am->mesh_count; i++)
    {
        if (strcmp(am->mesh_cache[i].name, name) == 0)
            return &am->mesh_cache[i];
    }

    return NULL;
}





// Look up a skinned mesh by its string name
SkinnedMesh* Asset_GetSkinnedMeshByName(AssetManager* am, const char* name)
{
    if (!am || !name)
        return NULL;

    for (uint32_t i = 0; i < am->skinned_mesh_count; i++)
    {
        if (strcmp(am->skinned_mesh_cache[i].name, name) == 0)
            return &am->skinned_mesh_cache[i];
    }

    return NULL;
}





// Look up a texture by its string name
Texture* Asset_GetTextureByName(AssetManager* am, const char* name)
{
    if (!am || !name)
        return NULL;

    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (strcmp(am->texture_cache[i].name, name) == 0)
            return &am->texture_cache[i];
    }

    return NULL;
}





// Look up a texture by its string name
Material* Asset_GetMaterialByName(AssetManager* am, const char* name)
{
    if (!am || !name)
        return NULL;

    for (uint32_t i = 0; i < am->material_count; i++)
    {
        if (strcmp(am->material_cache[i].name, name) == 0)
            return &am->material_cache[i];
    }

    return NULL;
}





// Look up a Font by its string name
Font* Asset_GetFontByName(AssetManager* am, const char* name)
{
    if (!am || !name)
        return NULL;

    for (uint32_t i = 0; i < am->font_count; i++)
    {
        if (strcmp(am->font_cache[i].name, name) == 0)
            return &am->font_cache[i];
    }

    return NULL;
}




















// Creates a texture from memory
Texture* Asset_CreateTextureFromMemory(AssetManager* am, const char* name, const unsigned char* buffer, int length)
{
    if (!am)
        return NULL;

    ImageData img = Image_LoadFromMemory(buffer, length, true);
    if (!img.pixels) return Asset_GetDefaultTexture(am);

    TextureHandle handle = Render_CreateTexture2D(am->renderer, img.pixels, img.width, img.height, img.channels);
    Image_Free(&img);

    // Cache it to not load the same texture twice
    Texture* t = FindFreeTextureSlot(am);
    if (t)
    {
        snprintf(t->name, sizeof(t->name), "%s", name);
        t->gpu_handle = handle;
        t->width = img.width;
        t->height = img.height;
        t->channels = img.channels;

        return t;
    }

    return Asset_GetDefaultTexture(am);
}










// Creates a texture from a color
Texture* Asset_CreateSolidColorTexture(AssetManager* am, const char* name, Color color)
{
    if (!am)
        return NULL;

    // Check if this specific color texture was already generated
    for (uint32_t i = 0; i < am->texture_count; i++)
    {
        if (strcmp(am->texture_cache[i].name, name) == 0)
            return &am->texture_cache[i]; 
    }

    // Convert the float (0.0 - 1.0) Color into 8-bit (0 - 255) pixel data
    unsigned char pixel[4] = {
        (unsigned char)(color.r * 255.0f),
        (unsigned char)(color.g * 255.0f),
        (unsigned char)(color.b * 255.0f),
        (unsigned char)(color.a * 255.0f)
    };

    // Create the texture from the renderer
    TextureHandle handle = Render_CreateTexture2D(am->renderer, pixel, 1, 1, 4);

    // Cache it exactly like a normal texture
    Texture* t = FindFreeTextureSlot(am);
    if (t)
    {
        snprintf(t->name, sizeof(t->name), "%s", name);
        t->gpu_handle = handle;
        t->width = 1;
        t->height = 1;
        t->channels = 4;
        
        return t;
    }
    
    return Asset_GetDefaultTexture(am);
}










// Makes the descriptor of a material
static RenderMaterialDesc Material_MakeDesc(Material* mat)
{
    RenderMaterialDesc desc = {0};
    desc.shader = DEFAULT_SHADER;

    if (mat->shader != NULL)
        desc.shader = mat->shader->gpu_handle;
    if (mat->albedo_texture)
        desc.albedo = mat->albedo_texture->gpu_handle;
    if (mat->normal_map)
        desc.normal = mat->normal_map->gpu_handle;
    if (mat->metallic_map)
        desc.metallic = mat->metallic_map->gpu_handle;
    if (mat->roughness_map)
        desc.roughness = mat->roughness_map->gpu_handle;
    if (mat->ao_map)
        desc.ao = mat->ao_map->gpu_handle;
    
    desc.properties = mat->properties;
    
    return desc;
}










// Syncs the GPU material with the cached material
void Asset_SyncMaterialGPU(AssetManager* am, Material* material)
{
    if (!am || !material || !am->renderer)
        return;
    
    RenderMaterialDesc desc = Material_MakeDesc(material);
    if (material->gpu_handle.id == 0)
    {
        material->gpu_handle = Render_CreateMaterial(am->renderer, &desc);
    }
    else if (material->id >= MAX_MATERIALS ||
             !am->material_gpu_desc_valid[material->id] ||
             memcmp(&am->material_gpu_desc[material->id], &desc, sizeof(desc)) != 0)
    {
        Render_UpdateMaterial(am->renderer, material->gpu_handle, &desc);
    }

    if (material->id < MAX_MATERIALS)
    {
        am->material_gpu_desc[material->id] = desc;
        am->material_gpu_desc_valid[material->id] = true;
    }
}










// Creates a material from a given shader and texture (diffuse)
Material* Asset_CreateMaterial(AssetManager* am, const char* name, Texture* albedo, Shader* shader)
{
    if (!am)
        return NULL;

    Material* mat = FindFreeMaterialSlot(am);
    if (!mat)
    {
        Log_Error("ERROR: Material pool full");
        return NULL;
    }
    
    snprintf(mat->name, sizeof(mat->name), "%s", name);
    mat->active = true;

    mat->shader = shader;
    
    if (albedo == NULL)
        mat->albedo_texture = Asset_GetDefaultTexture(am);
    else
        mat->albedo_texture = albedo;
    
    // Set other textures to NULL
    mat->normal_map = NULL;
    mat->metallic_map = NULL;
    mat->roughness_map = NULL;
    mat->ao_map = NULL;

    // Default physical properties
    mat->properties.albedo_tint = (Color){1.0f, 1.0f, 1.0f, 1.0f}; // Pure white
    mat->properties.metallic_factor = 0.0f;                        // Standard plastic
    mat->properties.roughness_factor = 0.5f;                       // Medium reflection
    mat->gpu_handle = (MaterialHandle){0};
    
    Asset_SyncMaterialGPU(am, mat);
    
    return mat;
}










// Creates a dynamic mesh from the renderer and returns the handle to the mesh
Mesh* Asset_CreateDynamicMesh(AssetManager* am, uint32_t max_vertices, uint32_t max_indices)
{
    if (!am)
        return NULL;

    Mesh* dynamic_mesh = FindFreeMeshSlot(am);
    if (!dynamic_mesh)
    {
        Log_Error("ERROR: Cannot create dynamic mesh. MAX_CACHED_MESHES (%d) reached.", MAX_CACHED_MESHES);
        return NULL;
    }

    MeshHandle handle = Render_CreateDynamicMesh(am->renderer, max_vertices, max_indices);

    snprintf(dynamic_mesh->name, sizeof(dynamic_mesh->name), "Dynamic_Mesh");
    dynamic_mesh->gpu_handle = handle;

    // TODO: Add proper local bounds to dynamic meshes when the renderer gets upgraded
    dynamic_mesh->local_bounds.min = (Vector3){-99999.0f, -99999.0f, -99999.0f};
    dynamic_mesh->local_bounds.max = (Vector3){ 99999.0f,  99999.0f,  99999.0f};
    dynamic_mesh->vertices = NULL;
    dynamic_mesh->vertex_count = 0;
    dynamic_mesh->indices = NULL;
    dynamic_mesh->index_count = 0;

    return dynamic_mesh;
}










// Updates a dynamic mesh from the renderer
void Asset_UpdateDynamicMesh(AssetManager* am, Mesh* mesh, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
{
    if (!am || !mesh)
        return;

    mesh->vertex_count = vertex_count;
    mesh->index_count = index_count;

    RenderMeshUpdate update = { vertices, vertex_count, indices, index_count };
    Render_UpdateMesh(am->renderer, mesh->gpu_handle, &update);
}










// Updates a static mesh with new vertices and indices
void Asset_UpdateMesh(AssetManager* am, Mesh* mesh, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
{
    if (!am || !mesh)
        return;

    mesh->vertex_count = vertex_count;
    mesh->index_count = index_count;

    if (vertices != mesh->vertices)
    {
        Vertex3D* new_verts = (Vertex3D*)malloc(vertex_count * sizeof(Vertex3D));
        memcpy(new_verts, vertices, vertex_count * sizeof(Vertex3D));
        if (mesh->vertices)
            free(mesh->vertices);
        
        mesh->vertices = new_verts;
    }

    if (indices != mesh->indices)
    {
        uint32_t* new_indices = (uint32_t*)malloc(index_count * sizeof(uint32_t));
        memcpy(new_indices, indices, index_count * sizeof(uint32_t));
        if (mesh->indices)
            free(mesh->indices);
        
        mesh->indices = new_indices;
    }

    Mesh_CalculateVertexTangents(mesh->vertices, mesh->vertex_count, mesh->indices, mesh->index_count);
    Mesh_CalculateBounds(mesh);

    RenderMeshUpdate update = { mesh->vertices, mesh->vertex_count, mesh->indices, mesh->index_count };
    Render_UpdateMesh(am->renderer, mesh->gpu_handle, &update);
}