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
#include "core/ioCore.h"
#include "core/meshCore.h"
#include "core/image.h"



#define MAX_CACHED_SHADERS 64
#define MAX_CACHED_TEXTURES 64
#define MAX_CACHED_MESHES 64
#define MAX_CACHED_MODELS 64
#define MAX_MATERIALS 1024



static Renderer* renderer = NULL;



static Shader shader_cache[MAX_CACHED_SHADERS];
static uint32_t shader_count = 0;

static Texture texture_cache[MAX_CACHED_TEXTURES];
static uint32_t texture_count = 0;

static Mesh mesh_cache[MAX_CACHED_MESHES];
static uint32_t mesh_count = 0;

static Model* model_cache[MAX_CACHED_MODELS];
static uint32_t model_count = 0;

static Material material_pool[MAX_MATERIALS];
static uint32_t material_count = 0;





static Mesh* builtin_quad = NULL;
static Mesh* builtin_cube = NULL;
static Mesh* builtin_sphere = NULL;
static Texture* default_texture = NULL;
static Shader* default_shader = NULL;





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





// Creates a texture from memory
static Texture* Asset_CreateTextureFromMemory(const char* name, const unsigned char* buffer, int length)
{
    ImageData img = Image_LoadFromMemory(buffer, length);
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





// Main model loader function
Model* Asset_LoadModel(const char* name, const char* filepath)
{
    // aiProcess_PreTransformVertices bakes all internal 3D offsets directly into the vertex data
    const struct aiScene* scene = aiImportFile(filepath, 
        aiProcess_Triangulate | 
        aiProcess_GenSmoothNormals | 
        aiProcess_FlipUVs |
        aiProcess_PreTransformVertices);

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

    // Create a temporary array to map Assimp material indices to Prism MaterialHandles
    Material** material_map = malloc(sizeof(Material*) * scene->mNumMaterials);

    Log_Info("ASSIMP: Scene has %d embedded textures.", scene->mNumTextures);
    for (uint32_t i = 0; i < scene->mNumMaterials; i++)
    {
        struct aiMaterial* ai_mat = scene->mMaterials[i];
        Texture* tex_ptr = Asset_GetDefaultTexture(); // Fallback

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
                Log_Info("ASSIMP: Found embedded texture. Width/Size: %d\n", embedded_tex->mWidth);
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
                    Log_Info("ASSIMP: Loading relative texture: %s", try_path_1);
                    tex_ptr = Asset_LoadTexture(clean_filename, try_path_1);
                }
                else 
                {
                    FILE* f2 = fopen(try_path_2, "rb");
                    if (f2) 
                    {
                        fclose(f2);
                        Log_Info("ASSIMP: Loading flattened texture: %s", try_path_2);
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
        Material* mat_ptr = Asset_CreateMaterial(Asset_GetDefaultShader(), tex_ptr);
        
        // Update the tint color to match the 3D file
        if (mat_ptr)
            mat_ptr->properties.tint_color = (Color){ diffuse_color.r, diffuse_color.g, diffuse_color.b, diffuse_color.a };

        material_map[i] = mat_ptr;
    }

    // Loop through every piece of the 3D file
    for (uint32_t m = 0; m < scene->mNumMeshes; m++)
    {
        struct aiMesh* ai_mesh = scene->mMeshes[m];
        
        uint32_t vertex_count = ai_mesh->mNumVertices;
        uint32_t index_count = ai_mesh->mNumFaces * 3; 

        Vertex3D* vertices = malloc(sizeof(Vertex3D) * vertex_count);
        uint32_t* indices = malloc(sizeof(uint32_t) * index_count);

        // Extract Vertices
        for (uint32_t i = 0; i < vertex_count; i++)
        {
            Vertex3D v;
            v.position = (Vector3){ ai_mesh->mVertices[i].x, ai_mesh->mVertices[i].y, ai_mesh->mVertices[i].z };
            v.normal = (Vector3){ ai_mesh->mNormals[i].x, ai_mesh->mNormals[i].y, ai_mesh->mNormals[i].z };

            if (ai_mesh->mTextureCoords[0])
                v.uv = (Vector2){ ai_mesh->mTextureCoords[0][i].x, ai_mesh->mTextureCoords[0][i].y };
            else
                v.uv = (Vector2){ 0.0f, 0.0f };

            vertices[i] = v;
        }

        // Extract Indices
        uint32_t index_offset = 0;
        for (uint32_t i = 0; i < ai_mesh->mNumFaces; i++)
        {
            struct aiFace face = ai_mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; j++)
            {
                indices[index_offset++] = face.mIndices[j];
            }
        }

        // Send this piece of the model to the GPU
        MeshHandle mesh_handle = Render_CreateMesh(renderer, vertices, vertex_count, indices, index_count);

        Mesh* sub_mesh = &mesh_cache[mesh_count];
        snprintf(sub_mesh->name, sizeof(sub_mesh->name), "%s_Part_%d", name, m);
        sub_mesh->id = mesh_count;
        sub_mesh->gpu_handle = mesh_handle;

        sub_mesh->vertices = vertices;
        sub_mesh->vertex_count = vertex_count;
        sub_mesh->indices = indices;
        sub_mesh->index_count = index_count;
        sub_mesh->local_bounds = CalculateAABB(vertices, vertex_count);

        mesh_count++;
        
        // Save it into the Model Node
        new_model->nodes[m].mesh = sub_mesh;
        
        // Use the correct material
        new_model->nodes[m].material = material_map[ai_mesh->mMaterialIndex];
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

    Mesh* new_mesh = &mesh_cache[mesh_count];

    // Cache mesh
    if (mesh_count < MAX_CACHED_MESHES)
    {
        strcpy(new_mesh->name, name);
        new_mesh->gpu_handle = handle;

        new_mesh->id = mesh_count;
        new_mesh->index_count = index_count;
        new_mesh->indices = final_indices;
        new_mesh->local_bounds = CalculateAABB(final_vertices, vertex_count);
        new_mesh->vertex_count = vertex_count;
        new_mesh->vertices = final_vertices;

        mesh_count++;
    }
    else
    {
        free(final_vertices);
        free(final_indices);
    }

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
Material* Asset_CreateMaterial(Shader* shader, Texture* diffuse)
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

    if (shader == NULL)
        mat->shader = Asset_GetDefaultShader();
    else
        mat->shader = shader;
    
    if (diffuse == NULL)
        mat->diffuse_texture = Asset_GetDefaultTexture();
    else
        mat->diffuse_texture = diffuse;
    
    
    // Default physical properties
    mat->properties.tint_color = (Color){1.0f, 1.0f, 1.0f, 1.0f}; // Pure white
    mat->properties.shininess = 32.0f;                        // Standard plastic
    mat->properties.specular_strength = 0.5f;                 // Medium reflection
    
    
    return mat;
}





// // Get a pointer to a material from a material handle
// Material* Asset_GetMaterial(MaterialHandle handle)
// {
//     if (handle.id < MAX_MATERIALS && material_pool[handle.id].active)
//         return &material_pool[handle.id];

//     return NULL;
// }





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
    ImageData img = Image_Load(filepath);
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
        m->local_bounds = CalculateAABB(heap_vertices, 4);

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
        m->local_bounds = CalculateAABB(heap_vertices, 24);

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
        m->local_bounds = CalculateAABB(vertices, vertex_count);

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
    
    unsigned char white_pixel[4] = {200, 200, 200, 255};

    TextureHandle handle = Render_CreateTexture(renderer, white_pixel, 1, 1, 4);

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





// Greate the standard shader
Shader* Asset_GetDefaultShader()
{
    // If already made return the handle
    if (default_shader != NULL)
        return default_shader;

    if (shader_count < MAX_CACHED_SHADERS)
    {
        default_shader = Asset_LoadShader("Default", "assets/shaders/default.vert", "assets/shaders/default.frag");
    }

    return default_shader;
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