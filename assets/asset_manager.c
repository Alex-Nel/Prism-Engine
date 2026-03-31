#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"
#include "asset_manager.h"
#include "core/io.h"
#include "core/meshCore.h"
#include "core/image.h"



#define MAX_CACHED_SHADERS 64
#define MAX_CACHED_TEXTURES 64
#define MAX_CACHED_MESHES 64
#define MAX_MATERIALS 1024



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

static Material material_pool[MAX_MATERIALS];
static uint32_t material_count = 0;





// We'll cache our builtin quad here too
static MeshHandle builtin_quad_handle = {0};
static bool is_quad_loaded = false;

static MeshHandle builtin_cube_handle = {0};
static bool is_cube_loaded = false;

static MeshHandle builtin_sphere_handle = {0};
static bool is_sphere_loaded = false;

static TextureHandle default_white_texture = {0};
static bool is_default_texture_loaded = false;

static ShaderHandle default_shader_handle = {0};
static bool is_default_shader_loaded = false;





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





MaterialHandle Asset_CreateMaterial(ShaderHandle shader, TextureHandle diffuse)
{
    if (material_count >= MAX_MATERIALS)
    {
        Log_Error("Material pool full!");
        return (MaterialHandle){0};
    }
    
    uint32_t id = material_count++;
    Material* mat = &material_pool[id];

    mat->id = id;
    mat->active = true;

    if (shader.id == 0)
        mat->shader_id = Asset_GetDefaultShader().id;
    else
        mat->shader_id = shader.id;
    
    if (diffuse.id == 0)
        mat->diffuse_texture_id = Asset_GetDefaultTexture().id;
    else
        mat->diffuse_texture_id = diffuse.id;
    
    // Default physical properties
    mat->properties.tint_color = (Vector3){1.0f, 1.0f, 1.0f}; // Pure white
    mat->properties.shininess = 32.0f;                        // Standard plastic
    mat->properties.specular_strength = 0.5f;                 // Medium reflection

    
    
    return (MaterialHandle){id};
}





Material* Asset_GetMaterial(MaterialHandle handle)
{
    if (handle.id < MAX_MATERIALS && material_pool[handle.id].active)
        return &material_pool[handle.id];

    return NULL;
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





MeshHandle Asset_GetBuiltinCube(void)
{
    if (is_cube_loaded) return builtin_cube_handle;

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

    builtin_cube_handle = Render_CreateMesh(vertices, 24, indices, 36);
    is_cube_loaded = true;
    
    // Cache it so it can be looked up by name if needed
    if (mesh_count < MAX_CACHED_MESHES) {
        strcpy(mesh_cache[mesh_count].name, "Cube");
        mesh_cache[mesh_count].handle = builtin_cube_handle;
        mesh_count++;
    }

    return builtin_cube_handle;
}





MeshHandle Asset_GetBuiltinSphere(void)
{
    if (is_sphere_loaded) return builtin_sphere_handle;

    // The resolution of the sphere. Higher = smoother but more memory.
    const int X_SEGMENTS = 32;
    const int Y_SEGMENTS = 32;
    const float PI = 3.14159265359f;

    uint32_t vertex_count = (X_SEGMENTS + 1) * (Y_SEGMENTS + 1);
    uint32_t index_count = X_SEGMENTS * Y_SEGMENTS * 6;

    Vertex3D* vertices = malloc(vertex_count * sizeof(Vertex3D));
    uint32_t* indices = malloc(index_count * sizeof(uint32_t));

    // --- 1. Generate Vertices ---
    uint32_t v_idx = 0;
    for (int y = 0; y <= Y_SEGMENTS; ++y) {
        for (int x = 0; x <= X_SEGMENTS; ++x) {
            float xSegment = (float)x / (float)X_SEGMENTS;
            float ySegment = (float)y / (float)Y_SEGMENTS;
            
            // Spherical coordinate math
            float xPos = cosf(xSegment * 2.0f * PI) * sinf(ySegment * PI);
            float yPos = cosf(ySegment * PI);
            float zPos = sinf(xSegment * 2.0f * PI) * sinf(ySegment * PI);

            Vertex3D v;
            v.position = (Vector3){xPos * 0.5f, yPos * 0.5f, zPos * 0.5f}; // Radius 0.5
            v.normal = (Vector3){xPos, yPos, zPos}; // Normal is just the normalized position!
            v.uv = (Vector2){xSegment, ySegment};

            vertices[v_idx++] = v;
        }
    }

    // --- 2. Generate Indices ---
    uint32_t i_idx = 0;
    for (int y = 0; y < Y_SEGMENTS; ++y) {
        for (int x = 0; x < X_SEGMENTS; ++x) {
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

    builtin_sphere_handle = Render_CreateMesh(vertices, vertex_count, indices, index_count);
    
    // Clean up our temporary CPU memory
    free(vertices);
    free(indices);

    is_sphere_loaded = true;

    if (mesh_count < MAX_CACHED_MESHES) {
        strcpy(mesh_cache[mesh_count].name, "Sphere");
        mesh_cache[mesh_count].handle = builtin_sphere_handle;
        mesh_count++;
    }

    return builtin_sphere_handle;
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





ShaderHandle Asset_GetDefaultShader()
{
    // If we already compiled it, just hand back the ID!
    if (is_default_shader_loaded)
        return default_shader_handle;

    // Otherwise, compile it straight from the hardcoded C-strings
    // default_shader_handle = Render_CreateShader(standard_vert_src, standard_frag_src);
    default_shader_handle = Asset_LoadShader("Default", "assets/shaders/default.vert", "assets/shaders/default.frag");
    
    is_default_shader_loaded = true;

    return default_shader_handle;
}