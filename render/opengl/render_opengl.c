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

    bool has_skybox;
    TextureHandle skybox_texture;
    ShaderHandle skybox_shader;
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





typedef struct OpenGL_Backend
{
    GLMesh mesh_pool[MAX_RESOURCES];
    GLShader shader_pool[MAX_RESOURCES];
    GLTexture texture_pool[MAX_RESOURCES];

    RenderCommand command_queue[MAX_COMMANDS];
    uint32_t command_count;

    GLuint depthMapFBO;
    GLuint depthMapTexture;

    RenderState state;

    uint32_t skybox_vao;
    uint32_t skybox_vbo;
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

    internal->state.window_width = width;
    internal->state.window_height = height;

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
}





// Draws the queued shadow casters with a specific light-space matrix.
static void OpenGL_DrawShadowQueue(OpenGL_Backend* internal, const Matrix4* light_space_matrix)
{
    uint32_t current_shader = 0;

    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];

        if (!internal->mesh_pool[cmd->mesh.id].active || !internal->shader_pool[cmd->shader.id].active)
            continue;

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];
        GLShader* gl_shader = &internal->shader_pool[cmd->shader.id];

        if (current_shader != cmd->shader.id)
        {
            glUseProgram(gl_shader->program);
            current_shader = cmd->shader.id;

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

    glBindFramebuffer(GL_FRAMEBUFFER, internal->depthMapFBO);

    // Render both faces into the depth map
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);

    for (uint32_t c = 0; c < cascade_count; c++)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->depthMapTexture, 0, c);
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glClear(GL_DEPTH_BUFFER_BIT);

        OpenGL_DrawShadowQueue(internal, &internal->state.light_space_matrices[c]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_POLYGON_OFFSET_FILL);
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
    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->depthMapTexture);
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
    internal->state.skybox_shader = packet->skybox_shader;
    
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

    // Render opaque objects
    for (uint32_t i = 0; i < transparent_start_idx; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];

        if (!internal->mesh_pool[cmd->mesh.id].active || !internal->shader_pool[cmd->shader.id].active)
            continue;

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];
        GLShader* gl_shader = &internal->shader_pool[cmd->shader.id];

        if (current_shader != cmd->shader.id)
        {
            glUseProgram(gl_shader->program);
            current_shader = cmd->shader.id;

            // Upload Camera Matrices
            GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
            GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
            glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
            glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);


            OpenGL_UploadShadowUniforms(gl_shader->program, &internal->state);


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

    if (blending_enabled)
    {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }

    glBindVertexArray(0);

    // Draw Skybox if it exists
    if (internal->state.has_skybox)
    {   
        uint32_t shader_id = internal->state.skybox_shader.id;
        uint32_t tex_id = internal->state.skybox_texture.id;

        if (shader_id != 0 || tex_id != 0)
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
            glBindVertexArray(internal->skybox_vao);
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
            if (!internal->mesh_pool[cmd->mesh.id].active || !internal->shader_pool[cmd->shader.id].active)
                continue;

            GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];
            GLShader* gl_shader = &internal->shader_pool[cmd->shader.id];

            if (current_shader != cmd->shader.id)
            {
                glUseProgram(gl_shader->program);
                current_shader = cmd->shader.id;

                // Upload Camera Matrices
                GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
                GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
                glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
                glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);


                OpenGL_UploadShadowUniforms(gl_shader->program, &internal->state);


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
Renderer* OpenGL_Init(Render_LoadProcFn load_proc)
{
    Renderer* r = malloc(sizeof(Renderer));
    if (!r)
        return NULL;

    memset(r, 0, sizeof(Renderer));

    OpenGL_Backend* internal = malloc(sizeof(OpenGL_Backend));
    memset(internal, 0, sizeof(OpenGL_Backend));


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

    glGenVertexArrays(1, &internal->skybox_vao);
    glGenBuffers(1, &internal->skybox_vbo);
    glBindVertexArray(internal->skybox_vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->skybox_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);



    // Generate depth map buffers

    glGenFramebuffers(1, &internal->depthMapFBO);
    glGenTextures(1, &internal->depthMapTexture);

    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->depthMapTexture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, SHADOW_WIDTH, SHADOW_HEIGHT, MAX_SHADOW_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    // GL_LINEAR on a depth texture with a compare mode set gives free 2x2 hardware
    // PCF (bilinear depth comparison) per tap, which the shader's sampler2DArrayShadow uses.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    
    // CLAMP_TO_EDGE avoids fetching the border color during PCF. Avoids edge pixels becoming falsely lit.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->depthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->depthMapTexture, 0, 0);

    // Tell OpenGL we are not writing colors to this buffer, only depth
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind back to default screen




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