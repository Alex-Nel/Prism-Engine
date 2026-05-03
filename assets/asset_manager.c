#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj.h"
#include "asset_manager.h"
#include "core/ioCore.h"
#include "core/meshCore.h"
#include "core/image.h"



#define MAX_CACHED_SHADERS 64
#define MAX_CACHED_TEXTURES 64
#define MAX_CACHED_MESHES 64
#define MAX_MATERIALS 1024



static Renderer* renderer = NULL;



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
    MeshData mesh_data;
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





void Asset_Init(Renderer* r)
{
    renderer = r;
    
    shader_count = 0;
    texture_count = 0;
    mesh_count = 0;
    material_count = 0;
}





// Calculates the AABB box for a specified mesh (set of vertices)
static AABB CalculateAABB(const Vertex3D* vertices, uint32_t vertex_count)
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





// Loads a mesh from a filepath (uses fastobj for obj files)
MeshHandle Asset_LoadMesh(const char* name, const char* filepath)
{
    // Check if mesh limit has been reached
    for (uint32_t i = 0; i < mesh_count; i++)
    {
        if (strcmp(mesh_cache[i].name, name) == 0)
            return mesh_cache[i].handle;
    }

    // Use fast_obj to read file (fast_obj automatically triangulates quads)
    fastObjMesh* mesh = fast_obj_read(filepath);
    if (!mesh)
    {
        Log_Warning("CRITICAL: Failed to load OBJ: %s\n", filepath);
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


    // Create GPU mesh
    MeshHandle handle = Render_CreateMesh(renderer, final_vertices, vertex_count, final_indices, index_count);


    // Cache mesh
    if (mesh_count < MAX_CACHED_MESHES)
    {
        strcpy(mesh_cache[mesh_count].name, name);
        mesh_cache[mesh_count].handle = handle;

        mesh_cache[mesh_count].mesh_data.vertices = final_vertices;
        mesh_cache[mesh_count].mesh_data.indices = final_indices;
        mesh_cache[mesh_count].mesh_data.vertex_count = vertex_count;
        mesh_cache[mesh_count].mesh_data.index_count = index_count;

        mesh_cache[mesh_count].mesh_data.local_bounds = CalculateAABB(final_vertices, vertex_count);

        mesh_count++;
    }
    else
    {
        free(final_vertices);
        free(final_indices);
    }

    fast_obj_destroy(mesh);


    return handle;
}





// Creates a material from a given shader and texture (diffuse)
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





// Get a pointer to a material from a material handle
Material* Asset_GetMaterial(MaterialHandle handle)
{
    if (handle.id < MAX_MATERIALS && material_pool[handle.id].active)
        return &material_pool[handle.id];

    return NULL;
}





// Loads a vertex and fragment shader from a file path and compiles it to a full shader
ShaderHandle Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path)
{
    // Check if we already loaded this exact shader
    for (uint32_t i = 0; i < shader_count; i++)
    {
        if (strcmp(shader_cache[i].name, name) == 0)
        {
            return shader_cache[i].handle;
        }
    }

    // Check if max shader count has been reached
    if (shader_count >= MAX_CACHED_SHADERS)
    {
        Log_Error("CRITICAL: Asset Manager out of shader cache space!\n");
        return (ShaderHandle){0};
    }


    // Read shaders from file
    char* v_src = IO_ReadTextFile(vert_path);
    char* f_src = IO_ReadTextFile(frag_path);


    if (!v_src || !f_src)
    {
        Log_Error("CRITICAL: Failed to read shader files for '%s'\n", name);
        if (v_src) free(v_src);
        if (f_src) free(f_src);
        return (ShaderHandle){0};
    }

    // Compile on GPU
    ShaderHandle new_handle = Render_CreateShader(renderer, v_src, f_src);

    // Free shaders from memory
    free(v_src);
    free(f_src);

    // Save shader to cacje
    strcpy(shader_cache[shader_count].name, name);
    shader_cache[shader_count].handle = new_handle;
    shader_count++;

    return new_handle;
}





// Loads a texture from a filepath
TextureHandle Asset_LoadTexture(const char* name, const char* filepath)
{
    // Check if the texture is already loaded
    for (uint32_t i = 0; i < texture_count; i++)
    {
        if (strcmp(texture_cache[i].name, name) == 0)
        {
            return texture_cache[i].handle;
        }
    }

    // Load pixels
    ImageData img = Image_Load(filepath);
    if (!img.pixels)
    {
        Log_Error("CRITICAL: Failed to load texture: %s\n", filepath);
        return (TextureHandle){0};
    }

    // Create mesh on GPU
    TextureHandle handle = Render_CreateTexture(renderer, img.pixels, img.width, img.height, img.channels);

    // Free image from memory
    Image_Free(&img);

    // Save image to cache
    if (texture_count < MAX_CACHED_TEXTURES)
    {
        strcpy(texture_cache[texture_count].name, name);
        texture_cache[texture_count].handle = handle;
        texture_count++;
    }

    return handle;
}









// Generate built in quad mesh (basic square)
MeshHandle Asset_GetBuiltinQuad()
{
    // If we already made the quad, return the ID
    if (is_quad_loaded)
    {
        return builtin_quad_handle;
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


    // Copy vertices and indices to the heap for caching
    Vertex3D* heap_vertices = malloc(sizeof(vertices));
    memcpy(heap_vertices, vertices, sizeof(vertices));
    uint32_t* heap_indices = malloc(sizeof(indices));
    memcpy(heap_indices, indices, sizeof(indices));

    builtin_quad_handle = Render_CreateMesh(renderer, heap_vertices, 4, heap_indices, 6);
    is_quad_loaded = true;

    // Cache the mesh
    if (mesh_count < MAX_CACHED_MESHES)
    {
        strcpy(mesh_cache[mesh_count].name, "Quad");
        mesh_cache[mesh_count].handle = builtin_quad_handle;

        mesh_cache[mesh_count].mesh_data.vertices = heap_vertices;
        mesh_cache[mesh_count].mesh_data.indices = heap_indices;
        mesh_cache[mesh_count].mesh_data.vertex_count = 4;
        mesh_cache[mesh_count].mesh_data.index_count = 6;

        mesh_cache[mesh_count].mesh_data.local_bounds = CalculateAABB(heap_vertices, 4);

        mesh_count++;
    }
    else
    {
        free(heap_vertices);
        free(heap_indices);
    }

    return builtin_quad_handle;
}





// Generate a standard cube mesh
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

    // Copy vertices and indices to the heap for caching
    Vertex3D* heap_vertices = malloc(sizeof(vertices));
    memcpy(heap_vertices, vertices, sizeof(vertices));
    uint32_t* heap_indices = malloc(sizeof(indices));
    memcpy(heap_indices, indices, sizeof(indices));


    builtin_cube_handle = Render_CreateMesh(renderer, heap_vertices, 24, heap_indices, 36);
    is_cube_loaded = true;
    
    // Cache the mesh
    if (mesh_count < MAX_CACHED_MESHES) {
        strcpy(mesh_cache[mesh_count].name, "Cube");
        mesh_cache[mesh_count].handle = builtin_cube_handle;

        mesh_cache[mesh_count].mesh_data.vertices = heap_vertices;
        mesh_cache[mesh_count].mesh_data.indices = heap_indices;
        mesh_cache[mesh_count].mesh_data.vertex_count = 24;
        mesh_cache[mesh_count].mesh_data.index_count = 36;

        mesh_cache[mesh_count].mesh_data.local_bounds = CalculateAABB(heap_vertices, 24);

        mesh_count++;
    }
    else
    {
        free(heap_vertices);
        free(heap_indices);
    }

    return builtin_cube_handle;
}





// Create a standard sphere
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


    // Create mesh and cache it if possible
    builtin_sphere_handle = Render_CreateMesh(renderer, vertices, vertex_count, indices, index_count);
    is_sphere_loaded = true;

    if (mesh_count < MAX_CACHED_MESHES) {
        strcpy(mesh_cache[mesh_count].name, "Sphere");
        mesh_cache[mesh_count].handle = builtin_sphere_handle;

        mesh_cache[mesh_count].mesh_data.vertices = vertices;
        mesh_cache[mesh_count].mesh_data.indices = indices;
        mesh_cache[mesh_count].mesh_data.vertex_count = vertex_count;
        mesh_cache[mesh_count].mesh_data.index_count = index_count;

        mesh_cache[mesh_count].mesh_data.local_bounds = CalculateAABB(vertices, vertex_count);

        mesh_count++;
    }
    else
    {
        free(vertices);
        free(indices);
    }

    return builtin_sphere_handle;
}











// Create the default texture (all white)
TextureHandle Asset_GetDefaultTexture()
{
    // If already made, return the handle
    if (is_default_texture_loaded)
        return default_white_texture;
    
    unsigned char white_pixel[4] = {200, 200, 200, 255};

    default_white_texture = Render_CreateTexture(renderer, white_pixel, 1, 1, 4);
    is_default_texture_loaded = true;

    return default_white_texture;
}





// Greate the standard shader
ShaderHandle Asset_GetDefaultShader()
{
    // If already made return the handle
    if (is_default_shader_loaded)
        return default_shader_handle;

    default_shader_handle = Asset_LoadShader("Default", "assets/shaders/default.vert", "assets/shaders/default.frag");    
    is_default_shader_loaded = true;

    return default_shader_handle;
}





MeshData* Asset_GetMeshData(MeshHandle handle)
{
    // Search the cache for the matching handle and return the pointer
    for (uint32_t i = 0; i < mesh_count; i++)
    {
        if (mesh_cache[i].handle.id == handle.id)
        {
            return &mesh_cache[i].mesh_data;
        }
    }
    return NULL; // return NULL if not found
}