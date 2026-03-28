#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"
#include "asset_manager.h"
#include "core/io.h"
#include "core/meshCore.h"
#include "core/image.h"



#define MAX_CACHED_SHADERS 64
#define MAX_CACHED_TEXTURES 64
#define MAX_CACHED_MESHES 64



// The internal cache structure
typedef struct CachedShader
{
    char name[64];
    ShaderHandle handle;
} CachedShader;



// The internal texture structure
typedef struct CachedTexture
{
    char name[64];
    TextureHandle handle;
} CachedTexture;



// The structure of a mesh
typedef struct CachedMesh
{
    char name[64];
    MeshHandle handle;
} CachedMesh;



static CachedShader shader_cache[MAX_CACHED_SHADERS];
static uint32_t shader_count = 0;

static CachedTexture texture_cache[MAX_CACHED_TEXTURES];
static uint32_t texture_count = 0;

static CachedMesh mesh_cache[MAX_CACHED_MESHES];
static uint32_t mesh_count = 0;



// We'll cache our builtin quad here too
static MeshHandle builtin_quad_handle = {0};
static bool is_quad_loaded = false;

static TextureHandle default_white_texture = {0};
static bool is_default_texture_loaded = false;



void Asset_Init(void)
{
    shader_count = 0;
    is_quad_loaded = false;
}





MeshHandle Asset_LoadMesh(const char* name, const char* filepath)
{
    // 1. Check cache
    for (uint32_t i = 0; i < mesh_count; i++)
    {
        if (strcmp(mesh_cache[i].name, name) == 0)
            return mesh_cache[i].handle;
    }

    // 2. Let fast_obj read the file
    // Note: fast_obj automatically triangulates quads for us!
    fastObjMesh* mesh = fast_obj_read(filepath);
    if (!mesh)
    {
        printf("CRITICAL: Failed to load OBJ: %s\n", filepath);
        return (MeshHandle){0};
    }

    uint32_t total_triangles = 0;
    for (unsigned int i = 0; i < mesh->face_count; ++i)
    {
        // A face with N vertices creates (N - 2) triangles.
        // E.g., a Quad (4) makes 2 triangles. A Triangle (3) makes 1.
        total_triangles += (mesh->face_vertices[i] - 2);
    }
    
    uint32_t vertex_count = total_triangles * 3;
    Vertex3D* final_vertices = malloc(sizeof(Vertex3D) * vertex_count);
    uint32_t* final_indices  = malloc(sizeof(uint32_t) * vertex_count);

    // --- The Triangulation Loop ---
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
        // Jump forward in the fast_obj array by the number of vertices we just processed
        index_offset += fv; 
    }

    // 5. Send to GPU
    MeshHandle handle = Render_CreateMesh(final_vertices, vertex_count, final_indices, vertex_count);

    // 6. Clean up CPU RAM
    free(final_vertices);
    free(final_indices);
    fast_obj_destroy(mesh);

    // 7. Cache it
    if (mesh_count < MAX_CACHED_MESHES) {
        strcpy(mesh_cache[mesh_count].name, name);
        mesh_cache[mesh_count].handle = handle;
        mesh_count++;
    }

    return handle;
}





ShaderHandle Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path)
{
    // 1. Check if we already loaded this exact shader
    for (uint32_t i = 0; i < shader_count; i++)
    {
        if (strcmp(shader_cache[i].name, name) == 0)
        {
            return shader_cache[i].handle; // Found it! Return the existing GPU ID.
        }
    }

    // 2. Not found. We must load it from disk.
    if (shader_count >= MAX_CACHED_SHADERS)
    {
        printf("CRITICAL: Asset Manager out of shader cache space!\n");
        return (ShaderHandle){0};
    }


    char* v_src = IO_ReadTextFile(vert_path);
    char* f_src = IO_ReadTextFile(frag_path);


    if (!v_src || !f_src)
    {
        printf("CRITICAL: Failed to read shader files for '%s'\n", name);
        if (v_src) free(v_src);
        if (f_src) free(f_src);
        return (ShaderHandle){0};
    }


    // 3. Compile on GPU
    ShaderHandle new_handle = Render_CreateShader(v_src, f_src);


    // 4. Free CPU RAM
    free(v_src);
    free(f_src);


    // 5. Save to our cache so we never have to read those files again
    strcpy(shader_cache[shader_count].name, name);
    shader_cache[shader_count].handle = new_handle;
    shader_count++;

    return new_handle;
}





// Add this declaration to asset_manager.h too!
TextureHandle Asset_LoadTexture(const char* name, const char* filepath)
{
    // 1. Check if we already loaded it
    for (uint32_t i = 0; i < texture_count; i++)
    {
        if (strcmp(texture_cache[i].name, name) == 0)
        {
            return texture_cache[i].handle;
        }
    }

    // 2. Load pixels to CPU RAM
    ImageData img = Image_Load(filepath);
    if (!img.pixels)
    {
        printf("CRITICAL: Failed to load texture: %s\n", filepath);
        return (TextureHandle){0};
    }

    // 3. Upload to GPU VRAM
    TextureHandle handle = Render_CreateTexture(img.pixels, img.width, img.height, img.channels);

    // 4. Free CPU RAM immediately! The GPU has the data now.
    Image_Free(&img);

    // 5. Save to cache
    if (texture_count < MAX_CACHED_TEXTURES)
    {
        strcpy(texture_cache[texture_count].name, name);
        texture_cache[texture_count].handle = handle;
        texture_count++;
    }

    return handle;
}





MeshHandle Asset_GetBuiltinQuad()
{
    // If we already made the quad, just hand back the ID!
    if (is_quad_loaded)
    {
        return builtin_quad_handle;
    }

    // Otherwise, generate it, send to GPU, and cache it.
    MeshData quad_cpu = Mesh_CreateQuad();
    
    builtin_quad_handle = Render_CreateMesh(
        quad_cpu.vertices, quad_cpu.vertex_count,
        quad_cpu.indices, quad_cpu.index_count
    );

    Mesh_FreeData(&quad_cpu);
    
    is_quad_loaded = true;
    return builtin_quad_handle;
}





TextureHandle Asset_GetDefaultTexture()
{
    if (is_default_texture_loaded)
        return default_white_texture;
    
    unsigned char white_pixel[4] = {255, 255, 255, 255};

    default_white_texture = Render_CreateTexture(white_pixel, 1, 1, 4);

    is_default_texture_loaded = true;

    return default_white_texture;
}