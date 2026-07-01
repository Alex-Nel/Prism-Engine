#include "../../include/glad/glad.h"
#include "../render.h"
#include "../../core/log.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>



#define MAX_DIR_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8

#define SHADOW_WIDTH SHADOW_MAP_RESOLUTION
#define SHADOW_HEIGHT SHADOW_MAP_RESOLUTION



// Struct for a global render state
typedef struct RenderState
{
    uint32_t window_width;
    uint32_t window_height;
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;

    DirectionalLightData dir_lights[MAX_DIR_LIGHTS];
    uint32_t dir_light_count;

    PointLightData point_lights[MAX_POINT_LIGHTS];
    uint32_t point_light_count;

    SpotLightData spot_lights[MAX_SPOT_LIGHTS];
    uint32_t spot_light_count;

    Matrix4 light_space_matrices[MAX_SHADOW_CASCADES];
    float shadow_texel_world_sizes[MAX_SHADOW_CASCADES];
    float cascade_splits[MAX_SHADOW_CASCADES - 1];
    Vector3 camera_forward;
    float shadow_camera_near;
    uint32_t shadow_cascade_count;
    float cascade_blend_fraction;

    bool enable_ssao;

    bool has_skybox;
    TextureHandle skybox_texture;
} RenderState;





// Struct for holding mesh data for OpenGL
typedef struct GLMesh
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    uint32_t index_count;
    bool active;
} GLMesh;


// Struct for holding shaders for OpenGL
typedef struct GLShader
{
    GLuint program;
    bool active;
} GLShader;


// Struct for holding textures for OpenGL
typedef struct GLTexture
{
    GLuint id;
    bool active;
} GLTexture;





// Struct for a render command. Contains mesh, shader, texture, material, and transform data
typedef struct RenderCommand
{
    MeshHandle mesh;
    ShaderHandle shader;
    TextureHandle texture;
    MaterialProperties mat_props;
    Matrix4 transform;
    Matrix4* bone_matrices;
    bool is_transparent;
    float depth_distance;
} RenderCommand;





// --- Sub-Systems for the render pipeline---

typedef struct GL_ForwardPipeline
{
    ShaderHandle default_shader;
    ShaderHandle animated_shader;
} GL_ForwardPipeline;


typedef struct GL_ShadowPipeline
{
    GLuint depthMapFBO;
    GLuint depthMapTextureArray;
    ShaderHandle static_shader;
    ShaderHandle skinned_shader;
} GL_ShadowPipeline;


typedef struct GL_SSAOPipeline
{
    GLuint gBufferFBO;
    GLuint gPosition;
    GLuint gNormal;
    GLuint gDepth;

    GLuint ssaoFBO;
    GLuint ssaoBlurFBO;
    GLuint ssaoColorBuffer;
    GLuint ssaoColorBufferBlur;

    Vector3 kernel[64];
    GLuint noiseTexture;
    GLuint fallbackWhiteTexture;

    ShaderHandle g_buffer_shader;
    ShaderHandle g_buffer_skinned_shader;
    ShaderHandle ssao_shader;
    ShaderHandle blur_shader;
} GL_SSAOPipeline;


typedef struct GL_SkyboxPipeline
{
    uint32_t vao;
    uint32_t vbo;
    ShaderHandle default_shader;
} GL_SkyboxPipeline;





// The main backend of the OpenGL renderer
typedef struct OpenGL_Backend
{
    GLMesh mesh_pool[MAX_RESOURCES];
    GLShader shader_pool[MAX_RESOURCES];
    GLTexture texture_pool[MAX_RESOURCES];

    RenderCommand command_queue[MAX_COMMANDS];
    uint32_t command_count;

    uint32_t quad_vao;
    uint32_t quad_vbo;
    
    RenderState state;

    GL_ForwardPipeline forward;
    GL_ShadowPipeline shadow;
    GL_SSAOPipeline   ssao;
    GL_SkyboxPipeline skybox;
} OpenGL_Backend;





// Shutdown function for OpenGL
static void OpenGL_Shutdown(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    // Clear out any pending draw commands
    internal->command_count = 0;

    // Garbage Collector Loop. We start at 1 because index 0 is the "Invalid/Null" handle.
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {    
        // Optional: You could add a LOG_WARN here to tell the user they had a memory leak during runtime!

        if (internal->mesh_pool[i].active)
            Render_DestroyMesh(r, (MeshHandle){i});
        
        if (internal->texture_pool[i].active)
            Render_DestroyTexture(r, (TextureHandle){i});
        
        if (internal->shader_pool[i].active)
            Render_DestroyShader(r, (ShaderHandle){i});
    }

    free(internal);
    free(r);
}










// Sets the size and position of the viewport
static void OpenGL_SetViewport(Renderer* r, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Prevent redundant reallocations if the size hasn't actually changed
    if (internal->state.window_width != width || internal->state.window_height != height)
    {
        internal->state.window_width = width;
        internal->state.window_height = height;

        // Resize G-Buffer Textures
        glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        // Resize G-Buffer Depth Renderbuffer
        glBindRenderbuffer(GL_RENDERBUFFER, internal->ssao.gDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        // Resize SSAO Textures
        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_FLOAT, NULL);
    }

    glViewport(x, y, width, height);
}

// Sets the color of the renderer to clear with
static void OpenGL_SetClearColor(Renderer* renderer, float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

// Clears all buffers in the context
static void OpenGL_Clear(Renderer* r)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Clears the depth buffer of openGL
static void OpenGL_ClearDepth(Renderer* r)
{
    // Wipe ONLY the depth buffer so the color from previous cameras remains!
    glClear(GL_DEPTH_BUFFER_BIT); 
}















// Rotates an images pixels by 90 degrees CW
static uint8_t* OpenGL_RotatePixels90CW(const uint8_t* src, int w, int h, int c)
{
    if (!src)
        return NULL;

    uint8_t* dest = (uint8_t*)malloc(w * h * c);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            // Calculate 90-degree clockwise coordinates
            int new_x = h - 1 - y;
            int new_y = x;

            int old_index = (y * w + x) * c;
            int new_index = (new_y * h + new_x) * c;

            for (int i = 0; i < c; i++)
                dest[new_index + i] = src[old_index + i];
        }
    }

    return dest;
}





// Rotates an image's pixels by 90 degrees CCW
static uint8_t* OpenGL_RotatePixels90CCW(const uint8_t* src, int w, int h, int c)
{
    if (!src)
        return NULL;

    uint8_t* dest = (uint8_t*)malloc(w * h * c);
    if (!dest)
        return NULL; // Good practice to check for allocation failure

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            // Calculate 90-degree counter-clockwise coordinates
            int new_x = y;
            int new_y = w - 1 - x;

            int old_index = (y * w + x) * c;
            int new_index = (new_y * h + new_x) * c;

            for (int i = 0; i < c; i++)
                dest[new_index + i] = src[old_index + i];
        }
    }

    return dest;
}










// Uploads vertex and index data to the GPU and returns a handle
static MeshHandle OpenGL_CreateMesh(Renderer* r, const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Find an empty slot
    // TODO: use a free-list for O(1) allocation
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->mesh_pool[i].active)
        {
            id = i;
            break;
        }
    }

    // Return 0 if pool is full
    if (id == 0) return (MeshHandle){0};

    GLMesh* mesh = &internal->mesh_pool[id];
    mesh->active = true;
    mesh->index_count = index_count;

    // Generate OpenGL buffers
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    glBindVertexArray(mesh->vao);

    // Upload Vertex Data
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex3D), vertices, GL_STATIC_DRAW);

    // Upload Index Data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

    // Define Vertex Attributes
    // Position (Vector3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
    glEnableVertexAttribArray(0);
    // Normal (Vector3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, normal));
    glEnableVertexAttribArray(1);
    // UV (Vector2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0); // Unbind to prevent accidental modifications

    return (MeshHandle){id};
}





// Uploads pixels to the renderer to make a texture. Returns a handle
static TextureHandle OpenGL_CreateTexture(Renderer* r, const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    if (!pixels) return (TextureHandle){0};

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Texture wrapping parameters (Repeat the image if UVs go past 1.0)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    if (width == 1 && height == 1)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Determine color format based on channels
    GLenum format;
    if (channels == 4)
        format = GL_RGBA;
    else
        format = GL_RGB;

    // Send pixels to GPU memory
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    
    if (width > 1 || height > 1)
    {
        glGenerateMipmap(GL_TEXTURE_2D); 
    }


    // Add openGL ID to the texture pool
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->texture_pool[i].active)
        {    
            // Store the raw OpenGL ID inside the pool
            internal->texture_pool[i].id = texture_id;
            internal->texture_pool[i].active = true;
            
            // Return the pool index
            return (TextureHandle){ i }; 
        }
    }

    // Fall back if no more texture slots
    glDeleteTextures(1, &texture_id);

    return (TextureHandle){ 0 };
}





// Uploads vertex and fragment shaders to the GPU to make a complete shader. Returns a handle
static ShaderHandle OpenGL_CreateShader(Renderer* r, const char* vertex_source, const char* fragment_source)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Check if another spot in the pool is available
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->shader_pool[i].active)
        {
            id = i;
            break;
        }
    }

    // Returns invalid handle if not
    if (id == 0) return (ShaderHandle){0};

    // Simplified compilation for brevity
    // TODO: check compile status further
    // Create vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_source, NULL);
    glCompileShader(vs);

    int success;
    char infoLog[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    
    if (!success)
    {
        glGetShaderInfoLog(vs, 512, NULL, infoLog);
        Log_Error("ERROR: Vertex Shader Compilation Failed!\n%s\n", infoLog);
    }

    // Create fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_source, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, NULL, infoLog);
        Log_Error("ERROR: Fragment Shader Compilation Failed!\n%s\n", infoLog);
    }

    // Create complete shader
    GLShader* shader = &internal->shader_pool[id];
    shader->active = true;
    shader->program = glCreateProgram();
    glAttachShader(shader->program, vs);
    glAttachShader(shader->program, fs);
    glLinkProgram(shader->program);

    glGetProgramiv(shader->program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shader->program, 512, NULL, infoLog);
        Log_Error("ERROR: Shader Program Linking Failed!\n%s\n", infoLog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return (ShaderHandle){id};
}





// Creates a CubeMap for the skybox. Returns a texture handle
static TextureHandle OpenGL_CreateCubemap(Renderer* r, const uint8_t* right, const uint8_t* left, const uint8_t* top, const uint8_t* bottom, const uint8_t* front, const uint8_t* back, uint32_t width, uint32_t height, uint32_t channels)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    
    // Find free texture slot in pool
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->texture_pool[i].active)
        {
            id = i; break;
        }
    }

    if (id == 0)
        return (TextureHandle){0};

    uint32_t gl_tex;
    glGenTextures(1, &gl_tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, gl_tex);

    const uint8_t* faces[6] = { right, left, top, bottom, front, back };
    
    // Assume GL_RGB for 3 channels, GL_RGBA for 4
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    if (left)   glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, left);
    if (right)  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, right);
    if (bottom) glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, bottom);
    if (back)   glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, back);
    if (front)  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, front);
    
    // Fix for the top being rotated CCW
    if (top)
    {
        uint8_t* rotated_top = OpenGL_RotatePixels90CCW(top, width, height, channels);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, rotated_top);

        free(rotated_top);
    }
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    internal->texture_pool[id].id = gl_tex;
    internal->texture_pool[id].active = true;

    return (TextureHandle){id};
}





// Uploads vertex and index data to the GPU and returns a handle
static MeshHandle OpenGL_CreateSkinnedMesh(Renderer* r, const Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Find an empty slot
    // TODO: use a free-list for O(1) allocation
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->mesh_pool[i].active)
        {
            id = i;
            break;
        }
    }

    // Return 0 if pool is full
    if (id == 0) return (MeshHandle){0};

    GLMesh* mesh = &internal->mesh_pool[id];
    mesh->active = true;
    mesh->index_count = index_count;

    // Generate OpenGL buffers
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    glBindVertexArray(mesh->vao);

    // Upload Vertex Data
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex3DSkinned), vertices, GL_STATIC_DRAW);

    // Upload Index Data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

    // Define Vertex Attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, position));
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, normal));
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, uv));

    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, bone_ids));
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, bone_weights));


    glBindVertexArray(0); // Unbind to prevent accidental modifications

    return (MeshHandle){id};
}





// Creates a dynamic mesh. Returns a mesh handle
static MeshHandle OpenGL_CreateDynamicMesh(Renderer* r, uint32_t max_vertices, uint32_t max_indices)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Find an empty slot
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->mesh_pool[i].active)
        {
            id = i;
            break;
        }
    }

    if (id == 0)
        return (MeshHandle){0};

    
    GLMesh* mesh = &internal->mesh_pool[id];
    mesh->active = true;
    mesh->index_count = 0;
    
    glGenVertexArrays(1, &mesh->vao);
    glGenBuffers(1, &mesh->vbo);
    glGenBuffers(1, &mesh->ebo);
    glBindVertexArray(mesh->vao);

    // Allocate empty vertex data with GL_DYNAMIC_DRAW
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, max_vertices * sizeof(Vertex3D), NULL, GL_DYNAMIC_DRAW);

    // Allocate empty index data with GL_DYNAMIC_DRAW
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, max_indices * sizeof(uint32_t), NULL, GL_DYNAMIC_DRAW);

    // Define Vertex Attributes
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
    
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, normal));
    
    // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, uv));

    glBindVertexArray(0);
    
    return (MeshHandle){id};
}





// Quickly overwrites the existing GPU memory with new vertex data
static void OpenGL_UpdateDynamicMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
{
    if (handle.id == 0 || handle.id >= MAX_RESOURCES)
        return;

    if (vertex_count == 0 || index_count == 0)
        return;

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    GLMesh* mesh = &internal->mesh_pool[handle.id];

    if (!mesh->active)
        return;

    mesh->index_count = index_count;

    // Overwrite the buffers
    glBindVertexArray(mesh->vao);

    // Vertex Buffer using SubData
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_count * sizeof(Vertex3D), vertices);

    // Overwrite the Index Buffer using SubData
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_count * sizeof(uint32_t), indices);

    glBindVertexArray(0);
}















// Removes a mesh from the GPU
static void OpenGL_DestroyMesh(Renderer* r, MeshHandle mesh)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Validate the handle to prevent segfaults
    if (mesh.id == 0 || mesh.id >= MAX_RESOURCES)
        return;

    GLMesh* gl_mesh = &internal->mesh_pool[mesh.id];
    
    // Only delete if the slot is actually in use
    if (gl_mesh->active)
    {
        // Tell OpenGL to free the GPU memory
        glDeleteVertexArrays(1, &gl_mesh->vao);
        glDeleteBuffers(1, &gl_mesh->vbo);
        glDeleteBuffers(1, &gl_mesh->ebo);
        
        // Mark the slot as free so Render_CreateMesh can reuse this ID later
        gl_mesh->active = false;
    }
}


// Removes a texture from the GPU
static void OpenGL_DestroyTexture(Renderer* r, TextureHandle texture)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Validate the handle to prevent segfaults
    if (texture.id == 0 || texture.id >= MAX_RESOURCES)
        return;

    GLTexture* tex = &internal->texture_pool[texture.id];

    // Only delete if the slot is actually in use
    if (tex->active)
    {
        // Tell OpenGL to free the GPU memory
        glDeleteTextures(1, &tex->id);

        // Mark the slot as free so Render_CreateMesh can reuse this ID later
        tex->active = false;
    }
}


// Removes a shader from the GPU
static void OpenGL_DestroyShader(Renderer* r, ShaderHandle shader)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Validate the handle to prevent segfaults
    if (shader.id == 0 || shader.id >= MAX_RESOURCES)
        return;

    GLShader* gl_shader = &internal->shader_pool[shader.id];

    // Only delete if the slot is actually in use
    if (gl_shader->active)
    {
        // Tell OpenGL to free the GPU memory
        glDeleteProgram(gl_shader->program);

        // Mark the slot as free so Render_CreateMesh can reuse this ID later
        gl_shader->active = false;
    }
}















// Helper function to compile an internal engine shader from raw source code strings
static ShaderHandle OpenGL_CompileInternalShader(OpenGL_Backend* internal, const char* name, const char* vertex_src, const char* fragment_src)
{
    // Find a free slot in the backend's shader pool
    uint32_t slot = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->shader_pool[i].active)
        {
            slot = i;
            break;
        }
    }
    
    if (slot == 0)
    {
        Log_Error("Failed to allocate internal shader: %s (Pool full)", name);
        return (ShaderHandle){0};
    }

    // Compile Vertex Shader
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertex_src, NULL);
    glCompileShader(vertex);
    
    int success;
    char infoLog[512];
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, NULL, infoLog);
        Log_Error("Internal Vertex Shader Compilation Failed (%s):\n%s", name, infoLog);
    }

    // Compile Fragment Shader
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragment_src, NULL);
    glCompileShader(fragment);
    
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, NULL, infoLog);
        Log_Error("Internal Fragment Shader Compilation Failed (%s):\n%s", name, infoLog);
    }

    // Link Program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        Log_Error("Internal Shader Linking Failed (%s):\n%s", name, infoLog);
        glDeleteProgram(program);
        return (ShaderHandle){0};
    }

    // Cleanup
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    // Store in backend
    internal->shader_pool[slot].program = program;
    internal->shader_pool[slot].active = true;

    return (ShaderHandle){slot};
}





// Helper to read files and compile an internal shader
static ShaderHandle OpenGL_CompileInternalShaderFromFile(OpenGL_Backend* internal, const char* name, const char* vert_path, const char* frag_path)
{
    char* vert_src = IO_ReadTextFile(vert_path);
    char* frag_src = IO_ReadTextFile(frag_path);

    if (!vert_src || !frag_src) {
        if (vert_src) free(vert_src);
        if (frag_src) free(frag_src);
        return (ShaderHandle){0};
    }

    // Call the compiler function we made previously
    ShaderHandle handle = OpenGL_CompileInternalShader(internal, name, vert_src, frag_src);

    free(vert_src);
    free(frag_src);

    return handle;
}





static void OpenGL_InitPipelines(OpenGL_Backend* internal)
{
    // 1. Forward Pipeline (Main Lit Shaders)
    internal->forward.default_shader = OpenGL_CompileInternalShaderFromFile(internal, "Default", "assets/shaders/default.vert", "assets/shaders/default.frag");
    internal->forward.animated_shader = OpenGL_CompileInternalShaderFromFile(internal, "Animated", "assets/shaders/animated.vert", "assets/shaders/default.frag");

    // 2. Shadow Pipeline
    internal->shadow.static_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Static", "assets/shaders/shadow.vert", "assets/shaders/shadow.frag");
    internal->shadow.skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Skinned", "assets/shaders/shadow_skinned.vert", "assets/shaders/shadow.frag");

    // 3. Skybox Pipeline
    internal->skybox.default_shader = OpenGL_CompileInternalShaderFromFile(internal, "Skybox", "assets/shaders/skybox_default.vert", "assets/shaders/skybox_default.frag");

    // 4. SSAO Pipeline
    internal->ssao.g_buffer_shader = OpenGL_CompileInternalShaderFromFile(internal, "G-Buffer", "assets/shaders/g_buffer.vert", "assets/shaders/g_buffer.frag");
    internal->ssao.g_buffer_skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "G-Buffer Skinned", "assets/shaders/g_buffer_skinned.vert", "assets/shaders/g_buffer.frag");
    internal->ssao.ssao_shader = OpenGL_CompileInternalShaderFromFile(internal, "SSAO Compute", "assets/shaders/ssao.vert", "assets/shaders/ssao.frag");
    internal->ssao.blur_shader = OpenGL_CompileInternalShaderFromFile(internal, "SSAO Blur", "assets/shaders/ssao.vert", "assets/shaders/ssao_blur.frag");

}





// Restores the default window framebuffer for forward rendering.
static void OpenGL_BindDefaultFramebuffer(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
}



// Binds the SSAO result (or a white fallback when SSAO is disabled).
static void OpenGL_BindSSAOTexture(OpenGL_Backend* internal, GLuint program)
{
    GLint ssao_loc = glGetUniformLocation(program, "ssaoMap");
    if (ssao_loc == -1)
        return;

    glUniform1i(ssao_loc, 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, internal->state.enable_ssao ? internal->ssao.ssaoColorBufferBlur : internal->ssao.fallbackWhiteTexture);
}





// Copies shadow-map state from a render packet into the backend.
static void OpenGL_CopyShadowState(RenderState* state, const RenderPacket* packet)
{
    state->shadow_cascade_count = packet->shadow_cascade_count;
   
    if (state->shadow_cascade_count < 1)
        state->shadow_cascade_count = 1;
    
    if (state->shadow_cascade_count > MAX_SHADOW_CASCADES)
        state->shadow_cascade_count = MAX_SHADOW_CASCADES;

    for (uint32_t i = 0; i < state->shadow_cascade_count; i++)
    {
        state->light_space_matrices[i] = packet->light_space_matrices[i];
        state->shadow_texel_world_sizes[i] = packet->shadow_texel_world_sizes[i];
    }

    for (uint32_t i = 1; i < state->shadow_cascade_count; i++)
        state->cascade_splits[i - 1] = packet->cascade_splits[i - 1];

    state->camera_forward = packet->camera_forward;
    state->shadow_camera_near = packet->shadow_camera_near;
    state->cascade_blend_fraction = packet->cascade_blend_fraction;
}





// Uploads cascaded shadow-map uniforms to a lit shader program.
static void OpenGL_UploadShadowUniforms(GLuint program, const RenderState* state)
{
    GLint loc = glGetUniformLocation(program, "u_LightSpaceMatrices");
    if (loc != -1)
        glUniformMatrix4fv(loc, (GLsizei)state->shadow_cascade_count, GL_FALSE, (float*)state->light_space_matrices);

    loc = glGetUniformLocation(program, "u_ShadowCascadeCount");
    if (loc != -1)
        glUniform1i(loc, (GLint)state->shadow_cascade_count);

    loc = glGetUniformLocation(program, "shadowMap");
    if (loc != -1)
        glUniform1i(loc, 1);

    loc = glGetUniformLocation(program, "u_ShadowTexelSizes");
    if (loc != -1)
        glUniform1fv(loc, (GLsizei)state->shadow_cascade_count, state->shadow_texel_world_sizes);

    if (state->shadow_cascade_count > 1)
    {
        loc = glGetUniformLocation(program, "u_CascadeSplits");
        if (loc != -1)
            glUniform1fv(loc, (GLsizei)(state->shadow_cascade_count - 1), state->cascade_splits);
        
        loc = glGetUniformLocation(program, "u_ShadowCameraNear");
        if (loc != -1)
            glUniform1f(loc, state->shadow_camera_near);

        loc = glGetUniformLocation(program, "u_CascadeBlendFraction");
        if (loc != -1)
            glUniform1f(loc, state->cascade_blend_fraction);
    }

    loc = glGetUniformLocation(program, "u_CameraForward");
    if (loc != -1)
        glUniform3fv(loc, 1, (float*)&state->camera_forward);

    loc = glGetUniformLocation(program, "u_ShadowMaxDistance");
    if (loc != -1)
    {
        // If there are no directional lights, fallback to a safe 200.0f units
        float max_dist = (state->dir_light_count > 0) ? state->dir_lights[0].shadow_max_distance : 200.0f;
        glUniform1f(loc, max_dist);
    }
}





// Draws the queued shadow casters with a specific light-space matrix.
static void OpenGL_DrawShadowQueue(OpenGL_Backend* internal, const Matrix4* light_space_matrix)
{
    uint32_t current_shader = 0;

    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];

        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];

        // Dynamically select the internal shadow shader based on skeleton presence
        ShaderHandle target_shader = (cmd->bone_matrices != NULL) ? internal->shadow.skinned_shader : internal->shadow.static_shader;
        GLShader* gl_shader = &internal->shader_pool[target_shader.id];

        if (current_shader != target_shader.id)
        {
            glUseProgram(gl_shader->program);
            current_shader = target_shader.id;

            GLint light_space_loc = glGetUniformLocation(gl_shader->program, "u_LightSpaceMatrix");
            if (light_space_loc != -1)
                glUniformMatrix4fv(light_space_loc, 1, GL_FALSE, (float*)light_space_matrix);
        }

        GLint model_loc = glGetUniformLocation(gl_shader->program, "u_Model");
        if (model_loc != -1)
            glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);

        GLint bone_loc = glGetUniformLocation(gl_shader->program, "u_BoneMatrices");
        if (bone_loc != -1 && cmd->bone_matrices != NULL)
            glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);

        glBindVertexArray(gl_mesh->vao);
        glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
    }
}





// Begins the shadow pass using a render packet
static void OpenGL_BeginShadowPass(Renderer* r, const RenderPacket* packet)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    OpenGL_CopyShadowState(&internal->state, packet);

    // Reset the command queue for the shadow pass
    internal->command_count = 0;
}





// Ends the shadow pass and resets the state
static void OpenGL_EndShadowPass(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    uint32_t cascade_count = internal->state.shadow_cascade_count;
    if (cascade_count < 1)
        cascade_count = 1;

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.depthMapFBO);

    // Render both faces into the depth map
    glDisable(GL_CULL_FACE);

    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    for (uint32_t c = 0; c < cascade_count; c++)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.depthMapTextureArray, 0, c);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);

        OpenGL_DrawShadowQueue(internal, &internal->state.light_space_matrices[c]);
    }

    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    internal->command_count = 0;
}





// Sets the global camera matrices for the current frame
static void OpenGL_BeginFrame(Renderer* r, const RenderPacket* packet)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    internal->state.window_width = packet->window_width;
    internal->state.window_height = packet->window_height;

    internal->state.view_matrix = packet->view_matrix;
    internal->state.projection_matrix = packet->projection_matrix;
    internal->state.camera_pos = packet->camera_pos;
    OpenGL_CopyShadowState(&internal->state, packet);

    // Bind the shadow map texture array to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
    glActiveTexture(GL_TEXTURE0);

    // Copy Directional Lights
    internal->state.dir_light_count = packet->dir_light_count;
    for (uint32_t i = 0; i < packet->dir_light_count; i++)
        internal->state.dir_lights[i] = packet->dir_lights[i];

    // Copy Point Lights
    internal->state.point_light_count = packet->point_light_count;
    for (uint32_t i = 0; i < packet->point_light_count; i++)
        internal->state.point_lights[i] = packet->point_lights[i];
        
    // Copy Spot Lights
    internal->state.spot_light_count = packet->spot_light_count;
    for (uint32_t i = 0; i < packet->spot_light_count; i++)
        internal->state.spot_lights[i] = packet->spot_lights[i];

    internal->state.has_skybox = packet->has_skybox;
    internal->state.skybox_texture = packet->skybox_texture;
    internal->state.enable_ssao = packet->enable_ssao;
    
    if (packet->skybox_shader.id != 0)
        internal->skybox.default_shader = packet->skybox_shader;

    // Reset the queue for the new frame
    internal->command_count = 0;
}



// Adds an object to the draw queue
static void OpenGL_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform, Matrix4* bone_matrices, bool is_transparent, float depth_distance)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Return if the queue is full
    if (internal->command_count >= MAX_COMMANDS) return;
    
    internal->command_queue[internal->command_count++] = (RenderCommand){
        mesh,
        shader,
        texture,
        mat_props,
        transform,
        bone_matrices,
        is_transparent,
        depth_distance
    };
}



// Compare render commands (for sorting)
static int CompareRenderCommands(const void* a, const void* b)
{
    RenderCommand* cmdA = (RenderCommand*)a;
    RenderCommand* cmdB = (RenderCommand*)b;

    // Opaque commands are first
    if (cmdA->is_transparent != cmdB->is_transparent)
        return cmdA->is_transparent - cmdB->is_transparent;

    // If transparent, sort back to front
    if (cmdA->is_transparent)
    {
        if (cmdA->depth_distance < cmdB->depth_distance) return 1;
        if (cmdA->depth_distance > cmdB->depth_distance) return -1;
        return 0;
    }

    // Sort primarily by shader, then by texture
    if (cmdA->shader.id != cmdB->shader.id)
        return (int)cmdA->shader.id - (int)cmdB->shader.id;
    
    return (int)cmdA->texture.id - (int)cmdB->texture.id;
}



// Sorts the queue, binds the state, and executes the actual GPU draw calls
static void OpenGL_EndFrame(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    glViewport(0, 0, internal->state.window_width, internal->state.window_height);

    // Sort the command queue
    qsort(internal->command_queue, internal->command_count, sizeof(RenderCommand), CompareRenderCommands);

    // Track current state to avoid redundant binds
    uint32_t current_shader = 0;
    uint32_t current_texture = 0;

    bool blending_enabled = false;

    glEnable(GL_CULL_FACE);

    uint32_t transparent_start_idx = internal->command_count;
    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        if (internal->command_queue[i].is_transparent)
        {
            transparent_start_idx = i;
            break;
        }
    }


    // Geometry pre-pass (g-buffer)
    if (internal->state.enable_ssao && internal->ssao.g_buffer_shader.id != 0 &&
        internal->ssao.ssao_shader.id != 0 && internal->ssao.blur_shader.id != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.gBufferFBO);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // Clear to black
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Track G-Buffer shader to avoid redundant uploads
        uint32_t current_g_shader = 0;

        for (uint32_t i = 0; i < transparent_start_idx; i++)
        {
            RenderCommand* cmd = &internal->command_queue[i];
            if (!internal->mesh_pool[cmd->mesh.id].active) continue;

            GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];

            // Dynamically select static vs skinned G-Buffer shader
            ShaderHandle target_g_handle = (cmd->bone_matrices != NULL) ? internal->ssao.g_buffer_skinned_shader : internal->ssao.g_buffer_shader;
            GLShader* g_prog = &internal->shader_pool[target_g_handle.id];

            if (current_g_shader != target_g_handle.id)
            {
                glUseProgram(g_prog->program);
                current_g_shader = target_g_handle.id;

                GLint view_loc = glGetUniformLocation(g_prog->program, "u_View");
                GLint proj_loc = glGetUniformLocation(g_prog->program, "u_Projection");
                glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
                glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);
            }

            // Upload Model Matrix
            GLint model_loc = glGetUniformLocation(g_prog->program, "u_Model");
            glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);

            // Upload Bones
            GLint bone_loc = glGetUniformLocation(g_prog->program, "u_BoneMatrices");
            if (bone_loc != -1 && cmd->bone_matrices != NULL)
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);

            glBindVertexArray(gl_mesh->vao);
            glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
        }



        // SSAO Pass

        glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoFBO);
        glClear(GL_COLOR_BUFFER_BIT);

        GLuint ssao_prog = internal->shader_pool[internal->ssao.ssao_shader.id].program;
        glUseProgram(ssao_prog);

        // Upload Matrices & Settings
        glUniformMatrix4fv(glGetUniformLocation(ssao_prog, "projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
        glUniform1i(glGetUniformLocation(ssao_prog, "kernelSize"), 16);
        glUniform1f(glGetUniformLocation(ssao_prog, "radius"), 0.5f); // Tweak this for AO distance!
        glUniform1f(glGetUniformLocation(ssao_prog, "bias"), 0.025f);

        // Upload the Kernel Array
        for (int k = 0; k < 64; ++k)
        {
            char var_name[32];
            sprintf(var_name, "samples[%d]", k);
            glUniform3fv(glGetUniformLocation(ssao_prog, var_name), 1, (float*)&internal->ssao.kernel[k]);
        }

        // Upload Noise Scale (Tile the 4x4 noise texture across the screen)
        float noise_x = (float)internal->state.window_width / 4.0f;
        float noise_y = (float)internal->state.window_height / 4.0f;
        glUniform2f(glGetUniformLocation(ssao_prog, "noiseScale"), noise_x, noise_y);

        // Bind G-Buffer Textures
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition);
        glUniform1i(glGetUniformLocation(ssao_prog, "gPosition"), 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
        glUniform1i(glGetUniformLocation(ssao_prog, "gNormal"), 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, internal->ssao.noiseTexture);
        glUniform1i(glGetUniformLocation(ssao_prog, "texNoise"), 2);

        // Draw Screen Quad
        glBindVertexArray(internal->quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);



        // SSAO Blur Pass
        
        glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoBlurFBO);
        glClear(GL_COLOR_BUFFER_BIT);

        GLuint blur_prog = internal->shader_pool[internal->ssao.blur_shader.id].program;
        glUseProgram(blur_prog);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer);
        glUniform1i(glGetUniformLocation(blur_prog, "ssaoInput"), 0);

        glBindVertexArray(internal->quad_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);


        OpenGL_BindDefaultFramebuffer();
    }

    OpenGL_BindDefaultFramebuffer();



    // --- Render opaque objects ---
    for (uint32_t i = 0; i < transparent_start_idx; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];

        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];

        ShaderHandle target_handle = cmd->shader;

        // If the material didn't have a shader, use the internal pipeline default
        if (target_handle.id == 0)
            target_handle = (cmd->bone_matrices != NULL) ? internal->forward.animated_shader : internal->forward.default_shader;

        // Now pull the actual OpenGL shader using the resolved handle
        GLShader* gl_shader = &internal->shader_pool[target_handle.id];

        // Ensure that the fallback shader actually exists
        if (!gl_shader->active)
            continue;

        if (current_shader != target_handle.id)
        {
            glUseProgram(gl_shader->program);
            current_shader = target_handle.id;

            // Upload Camera Matrices
            GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
            GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
            glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
            glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);


            OpenGL_UploadShadowUniforms(gl_shader->program, &internal->state);

            OpenGL_BindSSAOTexture(internal, gl_shader->program);

            GLint enable_ssao_loc = glGetUniformLocation(gl_shader->program, "u_EnableSSAO");
            if (enable_ssao_loc != -1)
                glUniform1i(enable_ssao_loc, internal->state.enable_ssao ? 1 : 0);


            // Upload Camera Position
            GLint view_pos_loc  = glGetUniformLocation(gl_shader->program, "u_ViewPos");
            if (view_pos_loc != -1)  glUniform3fv(view_pos_loc, 1, (float*)&internal->state.camera_pos);

            char uniform_name[64];

            // --- Upload Directional Lights ---
            glUniform1i(glGetUniformLocation(gl_shader->program, "u_DirLightCount"), internal->state.dir_light_count);
            for (uint32_t j = 0; j < internal->state.dir_light_count; j++)
            {
                DirectionalLightData* dl = &internal->state.dir_lights[j];
                
                sprintf(uniform_name, "u_DirLights[%d].direction", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&dl->direction);
                
                sprintf(uniform_name, "u_DirLights[%d].color", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&dl->color);
                
                sprintf(uniform_name, "u_DirLights[%d].intensity", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), dl->intensity);

                sprintf(uniform_name, "u_DirLights[%d].ambientStrength", j); // GLSL camelCase!
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), dl->ambient_strength);
            }

            // --- Upload Point Lights ---
            glUniform1i(glGetUniformLocation(gl_shader->program, "u_PointLightCount"), internal->state.point_light_count);
            for (uint32_t j = 0; j < internal->state.point_light_count; j++)
            {
                PointLightData* pl = &internal->state.point_lights[j];
                
                sprintf(uniform_name, "u_PointLights[%d].position", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&pl->position);
                
                sprintf(uniform_name, "u_PointLights[%d].color", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&pl->color);
                
                sprintf(uniform_name, "u_PointLights[%d].intensity", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->intensity);
                
                sprintf(uniform_name, "u_PointLights[%d].constant", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->constant);
                
                sprintf(uniform_name, "u_PointLights[%d].linear", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->linear);
                
                sprintf(uniform_name, "u_PointLights[%d].quadratic", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->quadratic);
            }

            // --- Upload Spot Lights ---
            glUniform1i(glGetUniformLocation(gl_shader->program, "u_SpotLightCount"), internal->state.spot_light_count);
            for (uint32_t j = 0; j < internal->state.spot_light_count; j++)
            {
                SpotLightData* sl = &internal->state.spot_lights[j];
                
                sprintf(uniform_name, "u_SpotLights[%d].position", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&sl->position);

                sprintf(uniform_name, "u_SpotLights[%d].direction", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&sl->direction);
                
                sprintf(uniform_name, "u_SpotLights[%d].color", j);
                glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&sl->color);
                
                sprintf(uniform_name, "u_SpotLights[%d].intensity", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->intensity);
                
                sprintf(uniform_name, "u_SpotLights[%d].constant", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->constant);
                
                sprintf(uniform_name, "u_SpotLights[%d].linear", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->linear);
                
                sprintf(uniform_name, "u_SpotLights[%d].quadratic", j);
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->quadratic);

                sprintf(uniform_name, "u_SpotLights[%d].cutOff", j); // GLSL camelCase!
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->inner_cut_off);

                sprintf(uniform_name, "u_SpotLights[%d].outerCutOff", j); // GLSL camelCase!
                glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->outer_cut_off);
            }
        }


        if (current_texture != cmd->texture.id)
        {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->texture.id].id);
            current_texture = cmd->texture.id;
        }

        // Upload Matrices
        GLint model_loc = glGetUniformLocation(gl_shader->program, "u_Model");
        
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);

        GLint diff_loc = glGetUniformLocation(gl_shader->program, "u_Material.diffuse");
        if (diff_loc != -1) glUniform1i(diff_loc, 0);

        GLint tint_loc = glGetUniformLocation(gl_shader->program, "u_Material.tint");
        if (tint_loc != -1) glUniform3fv(tint_loc, 1, (float*)&cmd->mat_props.tint_color);

        GLint shine_loc = glGetUniformLocation(gl_shader->program, "u_Material.shininess");
        if (shine_loc != -1) glUniform1f(shine_loc, cmd->mat_props.shininess);

        GLint spec_loc = glGetUniformLocation(gl_shader->program, "u_Material.specularStrength");
        if (spec_loc != -1) glUniform1f(spec_loc, cmd->mat_props.specular_strength);


        // Upload bone matrices
        GLint bone_loc = glGetUniformLocation(gl_shader->program, "u_BoneMatrices");
        if (bone_loc != -1) 
        {
            if (cmd->bone_matrices != NULL) 
            {
                // Upload matrices to the Animator
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);
            } 
            else 
            {
                // If the shader wants bones but the object has none, give it Identity matrices
                static Matrix4 identity_bones[MAX_BONES];
                static bool initialized = false;
                if (!initialized)
                {
                    for (int b = 0; b < MAX_BONES; b++)
                        identity_bones[b] = Matrix4Identity();
                        
                    initialized = true;
                }
                
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)identity_bones);
            }
        }
        

        // Draw
        glBindVertexArray(gl_mesh->vao);
        glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
    }


    glDepthFunc(GL_LESS);


    if (blending_enabled)
    {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    glBindVertexArray(0);

    // Draw Skybox if it exists
    if (internal->state.has_skybox)
    {   
        uint32_t shader_id = internal->skybox.default_shader.id;
        uint32_t tex_id = internal->state.skybox_texture.id;

        if (shader_id != 0 && tex_id != 0)
        {
            // Change depth func so it draws at max depth (1.0)
            glDepthFunc(GL_LEQUAL);
            glDisable(GL_CULL_FACE);
    
            GLuint prog = internal->shader_pool[shader_id].program;
            glUseProgram(prog);
            
            // Upload Camera Matrices
            GLint view_loc = glGetUniformLocation(prog, "u_View");
            GLint proj_loc = glGetUniformLocation(prog, "u_Projection");
            
            glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
            glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);
            
            // Bind the Cubemap from the Texture Pool
            uint32_t gl_tex_id = internal->texture_pool[internal->state.skybox_texture.id].id;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, gl_tex_id);
    
            // If your shader explicitly names the sampler, bind it
            GLint skybox_loc = glGetUniformLocation(prog, "u_Skybox");
            if (skybox_loc != -1) glUniform1i(skybox_loc, 0);
            
            // Draw all 36 vertices
            glBindVertexArray(internal->skybox.vao);
            glDrawArrays(GL_TRIANGLES, 0, 36); 
            glBindVertexArray(0);
            
            // Restore default depth testing
            glDepthFunc(GL_LESS);
            glEnable(GL_CULL_FACE);
        }

        // Reset current shader and texture before rendering transparent geometry
        current_shader = 0;
        current_texture = 0;
    }



    if (transparent_start_idx < internal->command_count)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // Protect depth buffer from transparent overlap

        glDisable(GL_CULL_FACE);

        for (uint32_t i = transparent_start_idx; i < internal->command_count; i++)
        {
            RenderCommand* cmd = &internal->command_queue[i];

            if (!internal->mesh_pool[cmd->mesh.id].active)
                continue;

            GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];

            ShaderHandle target_handle = cmd->shader;
        
            // If the material didn't have a shader, use the internal pipeline default!
            if (target_handle.id == 0)
                target_handle = (cmd->bone_matrices != NULL) ? internal->forward.animated_shader : internal->forward.default_shader;

            // Now pull the actual OpenGL shader using the resolved handle
            GLShader* gl_shader = &internal->shader_pool[target_handle.id];

            // Ensure the fallback shader actually exists and compiled properly
            if (!gl_shader->active)
                continue;

            if (current_shader != target_handle.id)
            {
                glUseProgram(gl_shader->program);
                current_shader = target_handle.id;

                // Upload Camera Matrices
                GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
                GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
                glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
                glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);


                OpenGL_UploadShadowUniforms(gl_shader->program, &internal->state);

                OpenGL_BindSSAOTexture(internal, gl_shader->program);

                GLint enable_ssao_loc = glGetUniformLocation(gl_shader->program, "u_EnableSSAO");
                if (enable_ssao_loc != -1)
                    glUniform1i(enable_ssao_loc, internal->state.enable_ssao ? 1 : 0);


                // Upload Camera Position
                GLint view_pos_loc  = glGetUniformLocation(gl_shader->program, "u_ViewPos");
                if (view_pos_loc != -1)  glUniform3fv(view_pos_loc, 1, (float*)&internal->state.camera_pos);

                char uniform_name[64];

                // --- Upload Directional Lights ---
                glUniform1i(glGetUniformLocation(gl_shader->program, "u_DirLightCount"), internal->state.dir_light_count);
                for (uint32_t j = 0; j < internal->state.dir_light_count; j++)
                {
                    DirectionalLightData* dl = &internal->state.dir_lights[j];
                    
                    sprintf(uniform_name, "u_DirLights[%d].direction", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&dl->direction);
                    
                    sprintf(uniform_name, "u_DirLights[%d].color", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&dl->color);
                    
                    sprintf(uniform_name, "u_DirLights[%d].intensity", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), dl->intensity);

                    sprintf(uniform_name, "u_DirLights[%d].ambientStrength", j); // GLSL camelCase!
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), dl->ambient_strength);
                }

                // --- Upload Point Lights ---
                glUniform1i(glGetUniformLocation(gl_shader->program, "u_PointLightCount"), internal->state.point_light_count);
                for (uint32_t j = 0; j < internal->state.point_light_count; j++)
                {
                    PointLightData* pl = &internal->state.point_lights[j];
                    
                    sprintf(uniform_name, "u_PointLights[%d].position", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&pl->position);
                    
                    sprintf(uniform_name, "u_PointLights[%d].color", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&pl->color);
                    
                    sprintf(uniform_name, "u_PointLights[%d].intensity", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->intensity);
                    
                    sprintf(uniform_name, "u_PointLights[%d].constant", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->constant);
                    
                    sprintf(uniform_name, "u_PointLights[%d].linear", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->linear);
                    
                    sprintf(uniform_name, "u_PointLights[%d].quadratic", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), pl->quadratic);
                }

                // --- Upload Spot Lights ---
                glUniform1i(glGetUniformLocation(gl_shader->program, "u_SpotLightCount"), internal->state.spot_light_count);
                for (uint32_t j = 0; j < internal->state.spot_light_count; j++)
                {
                    SpotLightData* sl = &internal->state.spot_lights[j];
                    
                    sprintf(uniform_name, "u_SpotLights[%d].position", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&sl->position);

                    sprintf(uniform_name, "u_SpotLights[%d].direction", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&sl->direction);
                    
                    sprintf(uniform_name, "u_SpotLights[%d].color", j);
                    glUniform3fv(glGetUniformLocation(gl_shader->program, uniform_name), 1, (float*)&sl->color);
                    
                    sprintf(uniform_name, "u_SpotLights[%d].intensity", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->intensity);
                    
                    sprintf(uniform_name, "u_SpotLights[%d].constant", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->constant);
                    
                    sprintf(uniform_name, "u_SpotLights[%d].linear", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->linear);
                    
                    sprintf(uniform_name, "u_SpotLights[%d].quadratic", j);
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->quadratic);

                    sprintf(uniform_name, "u_SpotLights[%d].cutOff", j); // GLSL camelCase!
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->inner_cut_off);

                    sprintf(uniform_name, "u_SpotLights[%d].outerCutOff", j); // GLSL camelCase!
                    glUniform1f(glGetUniformLocation(gl_shader->program, uniform_name), sl->outer_cut_off);
                }
            }


            if (current_texture != cmd->texture.id)
            {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->texture.id].id);
                current_texture = cmd->texture.id;
            }

            // Upload Matrices
            GLint model_loc = glGetUniformLocation(gl_shader->program, "u_Model");
            
            glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);

            GLint diff_loc = glGetUniformLocation(gl_shader->program, "u_Material.diffuse");
            if (diff_loc != -1) glUniform1i(diff_loc, 0);

            GLint tint_loc = glGetUniformLocation(gl_shader->program, "u_Material.tint");
            if (tint_loc != -1) glUniform3fv(tint_loc, 1, (float*)&cmd->mat_props.tint_color);

            GLint shine_loc = glGetUniformLocation(gl_shader->program, "u_Material.shininess");
            if (shine_loc != -1) glUniform1f(shine_loc, cmd->mat_props.shininess);

            GLint spec_loc = glGetUniformLocation(gl_shader->program, "u_Material.specularStrength");
            if (spec_loc != -1) glUniform1f(spec_loc, cmd->mat_props.specular_strength);


            // Upload bone matrices
            GLint bone_loc = glGetUniformLocation(gl_shader->program, "u_BoneMatrices");
            if (bone_loc != -1) 
            {
                if (cmd->bone_matrices != NULL) 
                {
                    // Upload matrices to the Animator
                    glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);
                } 
                else 
                {
                    // If the shader wants bones but the object has none, give it Identity matrices
                    static Matrix4 identity_bones[MAX_BONES];
                    static bool initialized = false;
                    if (!initialized)
                    {
                        for (int b = 0; b < MAX_BONES; b++)
                            identity_bones[b] = Matrix4Identity();
                            
                        initialized = true;
                    }
                    
                    glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)identity_bones);
                }
            }   

            // Draw
            glBindVertexArray(gl_mesh->vao);
            glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
        }

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE); // Restore depth writing
        glEnable(GL_CULL_FACE);
    }

    glBindVertexArray(0);
}





// OpenGL Initialization function
Renderer* OpenGL_Init(Render_LoadProcFn load_proc, uint32_t init_width, uint32_t init_height)
{
    Renderer* r = malloc(sizeof(Renderer));
    if (!r)
        return NULL;

    memset(r, 0, sizeof(Renderer));

    OpenGL_Backend* internal = malloc(sizeof(OpenGL_Backend));
    memset(internal, 0, sizeof(OpenGL_Backend));

    internal->state.window_width = init_width;
    internal->state.window_height = init_height;


    // Initialize data pools
    for (int i = 0; i < MAX_RESOURCES; i++)
    {
        internal->mesh_pool[i].active = false;
        internal->shader_pool[i].active = false;
        internal->texture_pool[i].active = false;
    }

    // Load OpenGL functions using the provided loader
    if (!gladLoadGLLoader((GLADloadproc)load_proc))
    {
        Log_Error("Failed to initialize OpenGL loader!");
        free(internal);
        free(r);
        return NULL;
    }

    // Set global OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE); // Disables drawing the inside of a mesh


    
    // Generate Internal Skybox VAO/VBO
    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,  -1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f, -1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f,  1.0f, -1.0f,   1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,   1.0f, -1.0f,  1.0f,  -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,   1.0f,  1.0f, -1.0f,   1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,  -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,  -1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &internal->skybox.vao);
    glGenBuffers(1, &internal->skybox.vbo);
    glBindVertexArray(internal->skybox.vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->skybox.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);



    // Generate Screen Quad
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
        1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
        1.0f, -1.0f,  1.0f, 0.0f,
        1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &internal->quad_vao);
    glGenBuffers(1, &internal->quad_vbo);
    glBindVertexArray(internal->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);



    // Generate depth map buffers

    glGenFramebuffers(1, &internal->shadow.depthMapFBO);
    glGenTextures(1, &internal->shadow.depthMapTextureArray);

    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT, MAX_SHADOW_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // GL_LINEAR on a depth texture with a compare mode set gives free 2x2 hardware
    // PCF (bilinear depth comparison) per tap, which the shader's sampler2DArrayShadow uses.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    
    // Use CLAMP_TO_BORDER so anything outside the cascade frustum is ignored
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.depthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.depthMapTextureArray, 0, 0);

    // Tell OpenGL we are not writing colors to this buffer, only depth
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind back to default screen



    // --- Generate G-Buffer ---

    uint32_t win_w = init_width;
    uint32_t win_h = init_height;

    glGenFramebuffers(1, &internal->ssao.gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.gBufferFBO);

    // Position color buffer (use RGBA16F for GPU alignment)
    glGenTextures(1, &internal->ssao.gPosition);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.gPosition, 0);

    // Normal color buffer
    glGenTextures(1, &internal->ssao.gNormal);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, internal->ssao.gNormal, 0);

    // Tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
    uint32_t attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // Create and attach depth buffer (renderbuffer)
    glGenRenderbuffers(1, &internal->ssao.gDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ssao.gDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, win_w, win_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, internal->ssao.gDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log_Error("G-Buffer Framebuffer not complete!");

    // --- Generate SSAO FBOs ---
    glGenFramebuffers(1, &internal->ssao.ssaoFBO);  
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoFBO);
    glGenTextures(1, &internal->ssao.ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, win_w/2, win_h/2, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer, 0);

    glGenFramebuffers(1, &internal->ssao.ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoBlurFBO);
    glGenTextures(1, &internal->ssao.ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, win_w/2, win_h/2, 0, GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur, 0);



    // Pre-fill SSAO buffers with white (1.0 = no occlusion) so the lit pass is safe before the first compute.
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoFBO);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoBlurFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    // 1x1 white texture used when SSAO is disabled in the forward pass.
    float ssao_fallback = 1.0f;
    glGenTextures(1, &internal->ssao.fallbackWhiteTexture);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.fallbackWhiteTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 1, 1, 0, GL_RED, GL_FLOAT, &ssao_fallback);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);



    // --- Generate SSAO Kernel & Noise Texture ---
    for (int i = 0; i < 64; ++i)
    {
        Vector3 sample = {
            ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f,
            ((float)rand() / (float)RAND_MAX) // Z is 0 to 1 to form a hemisphere
        };
        sample = Vector3Normalize(sample);
        
        // Push samples closer to the origin for better occlusion results
        float scale = (float)i / 64.0f;
        scale = 0.1f + (scale * scale) * (1.0f - 0.1f); // Lerp
        
        sample.x *= scale;
        sample.y *= scale;
        sample.z *= scale;
        internal->ssao.kernel[i] = sample;
    }

    float ssaoNoise[16 * 4];
    for (int i = 0; i < 16; i++)
    {
        ssaoNoise[i * 4 + 0] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        ssaoNoise[i * 4 + 1] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
        ssaoNoise[i * 4 + 2] = 0.0f;
        ssaoNoise[i * 4 + 3] = 0.0f;
    }
    
    glGenTextures(1, &internal->ssao.noiseTexture);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, ssaoNoise);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // MUST repeat to tile over screen
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    OpenGL_BindDefaultFramebuffer();


    // Initialize all pipelines
    OpenGL_InitPipelines(internal);




    r->backend_internal_data = internal;

    r->api = GRAPHICS_API_OPENGL;
    r->Shutdown = OpenGL_Shutdown;
    r->SetViewport = OpenGL_SetViewport;
    r->SetClearColor = OpenGL_SetClearColor;
    r->Clear = OpenGL_Clear;
    r->ClearDepth = OpenGL_ClearDepth;

    r->CreateMesh = OpenGL_CreateMesh;
    r->DestroyMesh = OpenGL_DestroyMesh;

    r->CreateTexture = OpenGL_CreateTexture;
    r->DestroyTexture = OpenGL_DestroyTexture;

    r->CreateShader = OpenGL_CreateShader;
    r->DestroyShader = OpenGL_DestroyShader;

    r->CreateCubemap = OpenGL_CreateCubemap;
    r->CreateDynamicMesh = OpenGL_CreateDynamicMesh;
    r->CreateSkinnedMesh = OpenGL_CreateSkinnedMesh;
    r->UpdateDynamicMesh = OpenGL_UpdateDynamicMesh;

    r->BeginShadowPass = OpenGL_BeginShadowPass;
    r->EndShadowPass = OpenGL_EndShadowPass;
    
    r->BeginFrame = OpenGL_BeginFrame;
    r->Submit = OpenGL_Submit;
    r->EndFrame = OpenGL_EndFrame;
    
    return r;
}