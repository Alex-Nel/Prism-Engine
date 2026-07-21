#include "../../include/glad/glad.h"
#include "../render.h"
#include "../../core/log.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>



#define MAX_DIR_LIGHTS 8
#define MAX_POINT_LIGHTS 512
#define MAX_SPOT_LIGHTS 512

#define MAX_SHADOW_CASTING_SPOTLIGHTS 8
#define MAX_SHADOW_CASTING_POINT_LIGHTS 8

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
    Matrix4 spot_light_matrices[MAX_SHADOW_CASTING_SPOTLIGHTS];

    float shadow_texel_world_sizes[MAX_SHADOW_CASCADES];
    float cascade_splits[MAX_SHADOW_CASCADES - 1];
    Vector3 camera_forward;
    float shadow_camera_near;
    uint32_t shadow_cascade_count;
    float cascade_blend_fraction;

    bool enable_ssao;
    Color global_ambient_color;
    float global_ambient_illumination;
    float gamma;
    RendererSettings settings;

    bool has_skybox;
    TextureHandle skybox_texture;
} RenderState;





// Struct for holding mesh data for OpenGL
typedef struct GLMesh
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    float bounding_radius;
    uint32_t index_count;
    uint32_t max_vertices;
    uint32_t max_indices;
    bool is_dynamic;
    bool is_skinned;
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

    TextureHandle albedo_map;
    TextureHandle normal_map;
    TextureHandle metallic_map;
    TextureHandle roughness_map;
    TextureHandle ao_map;
    
    MaterialProperties mat_props;
    Matrix4 transform;
    Matrix4* bone_matrices;
    bool is_transparent;
    float depth_distance;
    bool cast_shadows;
    bool receive_shadows;
} RenderCommand;





// --- Sub-Systems for the render pipeline---

typedef struct GL_ForwardPipeline
{
    ShaderHandle default_shader;
    ShaderHandle animated_shader;
} GL_ForwardPipeline;

typedef struct GL_DeferredPipeline
{
    ShaderHandle deferred_shader;
    ShaderHandle volume_shader;
    ShaderHandle spot_volume_shader;
    ShaderHandle post_shader;
    uint32_t lighting_fbo;
    uint32_t lighting_texture;
    uint32_t sphere_vao;
    uint32_t sphere_vbo;
    uint32_t sphere_ebo;
    uint32_t sphere_index_count;
} GL_DeferredPipeline;


typedef struct GL_ShadowPipeline
{
    // Directional Light Cascades
    GLuint depthMapFBO;
    GLuint depthMapTextureArray;

    // Spotlight shadows
    GLuint spotDepthMapFBO;
    GLuint spotDepthMapTextureArray;

    GLuint pointDepthMapFBO;
    GLuint pointDepthMaps[MAX_SHADOW_CASTING_POINT_LIGHTS];

    ShaderHandle static_shader;
    ShaderHandle skinned_shader;
    ShaderHandle point_static_shader;
    ShaderHandle point_skinned_shader;
} GL_ShadowPipeline;


typedef struct GL_SSAOPipeline
{
    GLuint gBufferFBO;
    GLuint gPosition;
    GLuint gNormal;
    GLuint gDepth;
    GLuint gAlbedoSpec;

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

    GL_ForwardPipeline  forward;
    GL_DeferredPipeline deferred;
    GL_ShadowPipeline   shadow;
    GL_SSAOPipeline     ssao;
    GL_SkyboxPipeline   skybox;

    GLuint default_white_texture;
    GLuint default_normal_texture;
    GLuint default_black_texture;
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.gAlbedoSpec);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        // Resize the linear HDR lighting target
        glBindTexture(GL_TEXTURE_2D, internal->deferred.lighting_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);

        // Resize G-Buffer Depth Renderbuffer
        glBindRenderbuffer(GL_RENDERBUFFER, internal->ssao.gDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);

        // Resize SSAO Textures (half resolution)
        uint32_t ssao_w = width / 2;
        uint32_t ssao_h = height / 2;
        if (ssao_w < 1) ssao_w = 1;
        if (ssao_h < 1) ssao_h = 1;

        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ssao_w, ssao_h, 0, GL_RED, GL_FLOAT, NULL);

        glBindTexture(GL_TEXTURE_2D, internal->ssao.ssaoColorBufferBlur);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ssao_w, ssao_h, 0, GL_RED, GL_FLOAT, NULL);
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
    mesh->max_vertices = vertex_count;
    mesh->max_indices = index_count;
    mesh->is_dynamic = false;
    mesh->is_skinned = false;

    float max_dist_sq = 0.0f;
    for (uint32_t i = 0; i < vertex_count; i++)
    {
        float x = vertices[i].position.x;
        float y = vertices[i].position.y;
        float z = vertices[i].position.z;
        
        float dist_sq = (x * x) + (y * y) + (z * z);
        
        if (dist_sq > max_dist_sq)
            max_dist_sq = dist_sq;
    }

    mesh->bounding_radius = sqrt(max_dist_sq);

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
    // Tangent (Vector3)
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, tangent));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0); // Unbind to prevent accidental modifications

    return (MeshHandle){id};
}





// Uploads pixels to the renderer to make a texture. Returns a handle
static TextureHandle OpenGL_CreateTexture(Renderer* r, const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    if (!pixels || width == 0 || height == 0 || channels == 0)
        return (TextureHandle){0};

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
    GLenum format = GL_RGBA;
    GLenum internal_format = GL_RGBA;
    if (channels == 1)
    {
        internal_format = GL_RED;
        format = GL_RED;
    }
    else if (channels == 2)
    {
        internal_format = GL_RG;
        format = GL_RG;
    }
    else if (channels == 3)
    {
        internal_format = GL_RGB;
        format = GL_RGB;
    }
    else if (channels == 4)
    {
        internal_format = GL_RGBA;
        format = GL_RGBA;
    }

    // Set unpack alignment to 1 byte so tight rows from stbi_load (especially 3-channel RGB or 1-channel RED) do not overflow bounds
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    
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
    
    
    // Determine color format based on channels
    GLenum format = GL_RGBA;
    GLenum internal_format = GL_RGBA;
    if (channels == 1) { internal_format = GL_RED; format = GL_RED; }
    else if (channels == 2) { internal_format = GL_RG; format = GL_RG; }
    else if (channels == 3) { internal_format = GL_RGB; format = GL_RGB; }
    else if (channels == 4) { internal_format = GL_RGBA; format = GL_RGBA; }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (left)   glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, left);
    if (right)  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, right);
    if (bottom) glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, bottom);
    if (back)   glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, back);
    if (front)  glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, front);


    // Fix for the top being rotated CCW
    if (top)
    {
        uint8_t* rotated_top = OpenGL_RotatePixels90CCW(top, width, height, channels);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, rotated_top);

        free(rotated_top);
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    
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
    mesh->max_vertices = vertex_count;
    mesh->max_indices = index_count;
    mesh->is_dynamic = false;
    mesh->is_skinned = true;

    float max_dist_sq = 0.0f;
    for (uint32_t i = 0; i < vertex_count; i++)
    {
        float x = vertices[i].position.x;
        float y = vertices[i].position.y;
        float z = vertices[i].position.z;
        
        float dist_sq = (x * x) + (y * y) + (z * z);
        
        if (dist_sq > max_dist_sq)
            max_dist_sq = dist_sq;
    }

    mesh->bounding_radius = sqrt(max_dist_sq);

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
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribIPointer(4, MAX_BONE_INFLUENCE, GL_INT, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, bone_ids));
    
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, MAX_BONE_INFLUENCE, GL_FLOAT, GL_FALSE, sizeof(Vertex3DSkinned), (void*)offsetof(Vertex3DSkinned, bone_weights));


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
    mesh->max_vertices = max_vertices;
    mesh->max_indices = max_indices;
    mesh->is_dynamic = true;
    mesh->is_skinned = false;
    
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

    // Tangent
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, tangent));

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

    // Vertex Buffer: reallocate if larger than max_vertices
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    if (vertex_count > mesh->max_vertices)
    {
        glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex3D), vertices, GL_DYNAMIC_DRAW);
        mesh->max_vertices = vertex_count;
    }
    else
    {
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_count * sizeof(Vertex3D), vertices);
    }


    // Index Buffer: reallocate if larger than max_indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    if (index_count > mesh->max_indices)
    {
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_DYNAMIC_DRAW);
        mesh->max_indices = index_count;
    }
    else
    {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, index_count * sizeof(uint32_t), indices);
    }

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
static ShaderHandle OpenGL_CompileInternalShader(OpenGL_Backend* internal, const char* name, const char* vertex_src, const char* geom_src, const char* fragment_src)
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

    int success;
    char infoLog[1024];

    // Compile Vertex Shader
    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vertex_src, NULL);
    glCompileShader(vertex);
    
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertex, 1024, NULL, infoLog);
        Log_Error("Internal Vertex Shader Compilation Failed (%s):\n%s", name, infoLog);
    }

    GLuint geometry = 0;
    if (geom_src != NULL)
    {
        geometry = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(geometry, 1, &geom_src, NULL);
        glCompileShader(geometry);
        
        glGetShaderiv(geometry, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(geometry, 1024, NULL, infoLog);
            Log_Error("Internal Geometry Shader Compilation Failed (%s):\n%s", name, infoLog);
        }
    }

    // Compile Fragment Shader
    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fragment_src, NULL);
    glCompileShader(fragment);
    
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragment, 1024, NULL, infoLog);
        Log_Error("Internal Fragment Shader Compilation Failed (%s):\n%s", name, infoLog);
    }

    // Link Program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    if (geometry != 0)
        glAttachShader(program, geometry); // Only attach if it exists
    glAttachShader(program, fragment);
    glLinkProgram(program);
    
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        Log_Error("Internal Shader Linking Failed (%s):\n%s", name, infoLog);
        glDeleteProgram(program);

        // Clean up shaders
        glDeleteShader(vertex);
        if (geometry != 0) glDeleteShader(geometry);
        glDeleteShader(fragment);

        return (ShaderHandle){0};
    }

    // Cleanup
    glDeleteShader(vertex);
    if (geometry != 0)
        glDeleteShader(geometry);
    glDeleteShader(fragment);

    // Store in backend
    internal->shader_pool[slot].program = program;
    internal->shader_pool[slot].active = true;

    return (ShaderHandle){slot};
}





// Helper to read files and compile an internal shader
static ShaderHandle OpenGL_CompileInternalShaderFromFile(OpenGL_Backend* internal, const char* name, const char* vert_path, const char* geom_path, const char* frag_path)
{
    char* vert_src = IO_ReadTextFile(vert_path);
    char* frag_src = IO_ReadTextFile(frag_path);
    char* geom_src = geom_path ? IO_ReadTextFile(geom_path) : NULL;

    if (!vert_src || !frag_src || (geom_path && !geom_src))
    {
        if (vert_src) free(vert_src);
        if (frag_src) free(frag_src);
        if (geom_src) free(geom_src);

        return (ShaderHandle){0};
    }

    // Call the compiler function we made previously
    ShaderHandle handle = OpenGL_CompileInternalShader(internal, name, vert_src, geom_src, frag_src);

    free(vert_src);
    free(frag_src);
    if (geom_src) free(geom_src);

    return handle;
}





static void OpenGL_InitPipelines(OpenGL_Backend* internal)
{
    // 1. Forward Pipeline (Main Lit Shaders)
    internal->forward.default_shader = OpenGL_CompileInternalShaderFromFile(internal, "Foward Default", "assets/shaders/default.vert", NULL, "assets/shaders/default.frag");
    internal->forward.animated_shader = OpenGL_CompileInternalShaderFromFile(internal, "Forward Animated", "assets/shaders/animated.vert", NULL, "assets/shaders/default.frag");
    
    // 2. Deferred Pipeline
    internal->deferred.deferred_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Main", "assets/shaders/deferred_light.vert", NULL, "assets/shaders/deferred_light.frag");
    internal->deferred.volume_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Volume", "assets/shaders/deferred_volume.vert", NULL, "assets/shaders/deferred_volume.frag");
    internal->deferred.spot_volume_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Spot Volume", "assets/shaders/deferred_volume.vert", NULL, "assets/shaders/deferred_spot_volume.frag");
    internal->deferred.post_shader = OpenGL_CompileInternalShaderFromFile(internal, "Deferred Post", "assets/shaders/deferred_light.vert", NULL, "assets/shaders/deferred_post.frag");

    // 3. Shadow Pipeline
    internal->shadow.static_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Static", "assets/shaders/shadow.vert", NULL, "assets/shaders/shadow.frag");
    internal->shadow.skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Skinned", "assets/shaders/shadow_skinned.vert", NULL, "assets/shaders/shadow.frag");
    internal->shadow.point_static_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Point Static", "assets/shaders/shadow_point_static.vert", "assets/shaders/shadow_point.geom", "assets/shaders/shadow_point.frag");
    internal->shadow.point_skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "Shadow Point Skinned", "assets/shaders/shadow_point_skinned.vert", "assets/shaders/shadow_point.geom", "assets/shaders/shadow_point.frag");

    // 4. Skybox Pipeline
    internal->skybox.default_shader = OpenGL_CompileInternalShaderFromFile(internal, "Skybox", "assets/shaders/skybox.vert", NULL, "assets/shaders/skybox.frag");

    // 5. SSAO Pipeline
    internal->ssao.g_buffer_shader = OpenGL_CompileInternalShaderFromFile(internal, "G-Buffer", "assets/shaders/g_buffer.vert", NULL, "assets/shaders/g_buffer.frag");
    internal->ssao.g_buffer_skinned_shader = OpenGL_CompileInternalShaderFromFile(internal, "G-Buffer Skinned", "assets/shaders/g_buffer_skinned.vert", NULL, "assets/shaders/g_buffer.frag");
    internal->ssao.ssao_shader = OpenGL_CompileInternalShaderFromFile(internal, "SSAO Compute", "assets/shaders/ssao.vert", NULL, "assets/shaders/ssao.frag");
    internal->ssao.blur_shader = OpenGL_CompileInternalShaderFromFile(internal, "SSAO Blur", "assets/shaders/ssao.vert", NULL, "assets/shaders/ssao_blur.frag");
}




















// Restores the default window framebuffer for forward rendering.
static void OpenGL_BindDefaultFramebuffer()
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

    glUniform1i(ssao_loc, 5);
    glActiveTexture(GL_TEXTURE5);
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

        if (cmd->mesh.id == 0 || cmd->mesh.id >= MAX_RESOURCES || !internal->mesh_pool[cmd->mesh.id].active)
            continue;

        if (!cmd->cast_shadows)
            continue;

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];

        // Dynamically select the internal shadow shader based on skeleton presence and skinned mesh format
        ShaderHandle target_shader = (cmd->bone_matrices != NULL && gl_mesh->is_skinned) ? internal->shadow.skinned_shader : internal->shadow.static_shader;
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
        if (bone_loc != -1)
        {
            if (cmd->bone_matrices != NULL && gl_mesh->is_skinned)
            {
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);
            }
            else
            {
                static Matrix4 identity_bones[MAX_BONES];
                static bool initialized = false;
                if (!initialized)
                {
                    for (int b = 0; b < MAX_BONES; b++) identity_bones[b] = Matrix4Identity();
                    initialized = true;
                }
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)identity_bones);
            }
        }

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

    // --- Directional Light Cascades ---

    uint32_t cascade_count = internal->state.shadow_cascade_count;
    if (cascade_count < 1)
        cascade_count = 1;

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.depthMapFBO);

    // Render both faces into the depth map
    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    uint32_t shadow_res = SHADOW_WIDTH;
    if (internal->state.settings.shadow_map_resolution > 0)
        shadow_res = internal->state.settings.shadow_map_resolution;
    
    for (uint32_t c = 0; c < cascade_count; c++)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.depthMapTextureArray, 0, c);
        glViewport(0, 0, shadow_res, shadow_res);
        glClear(GL_DEPTH_BUFFER_BIT);

        OpenGL_DrawShadowQueue(internal, &internal->state.light_space_matrices[c]);
    }



    // --- SpotLight Shadows ---

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.spotDepthMapFBO);
    
    // We only process up to the max limit for shadow-casting spotlights
    int shadow_spot_index = 0; 
    
    for (uint32_t i = 0; i < internal->state.spot_light_count; i++)
    {
        if (shadow_spot_index >= MAX_SHADOW_CASTING_SPOTLIGHTS)
            break; // Hard cap
        
        SpotLightData* sl = &internal->state.spot_lights[i];

        if (!sl->casts_shadows)
            continue;

        // Make the Perspective Projection for the Spotlight. We use the outer_cut_off (in degrees) * 2 to get the full FOV of the cone
        // float fov_rad = (sl->outer_cut_off * 2.0f) * (3.14159265359f / 180.0f);
        float fov_rad = acosf(sl->outer_cut_off) * 2.0f;
        Matrix4 spotProj = Matrix4Perspective(fov_rad, 1.0f, 0.1f, 100.0f); // Adjust 100.0f to max distance - TODO: investigate this

        // Create the View Matrix
        Vector3 target = {
            sl->position.x + sl->direction.x,
            sl->position.y + sl->direction.y,
            sl->position.z + sl->direction.z
        };

        // If the light points straight down, use a different UP vector to prevent matrix singularity.
        Vector3 up = {0.0f, 1.0f, 0.0f};
        if (fabsf(sl->direction.y) > 0.99f)
            up = (Vector3){0.0f, 0.0f, 1.0f};
        
        Matrix4 spotView = Matrix4LookAt(sl->position, target, up);
        
        // Save the matrix so we can pass it to the deferred shader later
        internal->state.spot_light_matrices[shadow_spot_index] = Matrix4Multiply(spotProj, spotView);

        // Render to the specific layer of the Texture Array
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.spotDepthMapTextureArray, 0, shadow_spot_index);
        glViewport(0, 0, 1024, 1024);
        glClear(GL_DEPTH_BUFFER_BIT);
        
        OpenGL_DrawShadowQueue(internal, &internal->state.spot_light_matrices[shadow_spot_index]);
        
        shadow_spot_index++;
    }



    // --- Point Light Shadows ---

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.pointDepthMapFBO);
    glViewport(0, 0, 1024, 1024);
    
    int shadow_point_index = 0; 
    uint32_t current_point_shader = 0;

    for (uint32_t i = 0; i < internal->state.point_light_count; i++)
    {
        if (shadow_point_index >= MAX_SHADOW_CASTING_POINT_LIGHTS)
            break;

        PointLightData* pl = &internal->state.point_lights[i];

        if (!pl->casts_shadows)
            continue;

        // glFramebufferTexture allows the Geometry shader to route to the 6 faces via gl_Layer
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.pointDepthMaps[shadow_point_index], 0);        
        glClear(GL_DEPTH_BUFFER_BIT);

        // 90 Degree FOV forms a perfect box. Aspect ratio is 1.0f.
        float fov_rad = 90.0f * (3.14159265359f / 180.0f);
        Matrix4 shadowProj = Matrix4Perspective(fov_rad, 1.0f, 0.1f, 100.0f); // Adjust 100.0f to max distance
        Vector3 pos = pl->position;

        // The 6 directions of a Cubemap (Right, Left, Top, Bottom, Near, Far)
        Matrix4 shadowTransforms[6];
        shadowTransforms[0] = Matrix4Multiply(shadowProj, Matrix4LookAt(pos, (Vector3){pos.x + 1.0f, pos.y, pos.z}, (Vector3){0.0f, -1.0f, 0.0f}));
        shadowTransforms[1] = Matrix4Multiply(shadowProj, Matrix4LookAt(pos, (Vector3){pos.x - 1.0f, pos.y, pos.z}, (Vector3){0.0f, -1.0f, 0.0f}));
        shadowTransforms[2] = Matrix4Multiply(shadowProj, Matrix4LookAt(pos, (Vector3){pos.x, pos.y + 1.0f, pos.z}, (Vector3){0.0f,  0.0f,  1.0f}));
        shadowTransforms[3] = Matrix4Multiply(shadowProj, Matrix4LookAt(pos, (Vector3){pos.x, pos.y - 1.0f, pos.z}, (Vector3){0.0f,  0.0f, -1.0f}));
        shadowTransforms[4] = Matrix4Multiply(shadowProj, Matrix4LookAt(pos, (Vector3){pos.x, pos.y, pos.z + 1.0f}, (Vector3){0.0f, -1.0f, 0.0f}));
        shadowTransforms[5] = Matrix4Multiply(shadowProj, Matrix4LookAt(pos, (Vector3){pos.x, pos.y, pos.z - 1.0f}, (Vector3){0.0f, -1.0f, 0.0f}));

        // Get the radius of the light
        float max_color = fmaxf(fmaxf(pl->color.r, pl->color.g), pl->color.b);
        float light_radius = 50.0f; // Safe fallback if attenuation is 0
        
        if (pl->quadratic > 0.0001f)
            light_radius = (-pl->linear + sqrtf(pl->linear * pl->linear - 4 * pl->quadratic * (pl->constant - (256.0f / 5.0f) * max_color))) / (2.0f * pl->quadratic);
        else if (pl->linear > 0.0001f)
            light_radius = ((256.0f / 5.0f) * max_color - pl->constant) / pl->linear;


        // Draw the queue using our specialized point_shader
        for (uint32_t c = 0; c < internal->command_count; c++)
        {
            RenderCommand* cmd = &internal->command_queue[c];
            
            if (cmd->mesh.id == 0 || cmd->mesh.id >= MAX_RESOURCES || !internal->mesh_pool[cmd->mesh.id].active)
                continue;

            if (!cmd->cast_shadows)
                continue;


            // Extract Position from transform Matrix
            Vector3 mesh_center = {
                cmd->transform.m03, 
                cmd->transform.m13, 
                cmd->transform.m23
            };

            // Extract Max Scale by checking the magnitude of the X, Y, and Z axis vectors
            float scale_x = sqrtf(cmd->transform.m00*cmd->transform.m00 + cmd->transform.m10*cmd->transform.m10 + cmd->transform.m20*cmd->transform.m20);
            float scale_y = sqrtf(cmd->transform.m01*cmd->transform.m01 + cmd->transform.m11*cmd->transform.m11 + cmd->transform.m21*cmd->transform.m21);
            float scale_z = sqrtf(cmd->transform.m02*cmd->transform.m02 + cmd->transform.m12*cmd->transform.m12 + cmd->transform.m22*cmd->transform.m22);
            float max_scale = fmaxf(scale_x, fmaxf(scale_y, scale_z));

            // Calculate final world-space radius
            float world_bounding_radius = internal->mesh_pool[cmd->mesh.id].bounding_radius * max_scale;

            // Calculate Distance
            float dx = mesh_center.x - pos.x;
            float dy = mesh_center.y - pos.y;
            float dz = mesh_center.z - pos.z;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);

            // If the mesh is outside the light's reach, skip the Geometry Shader entirely
            if (dist > (light_radius + world_bounding_radius))
                continue;


            ShaderHandle target_shader = internal->shadow.point_static_shader; 
            if (cmd->bone_matrices != NULL && internal->mesh_pool[cmd->mesh.id].is_skinned) 
                target_shader = internal->shadow.point_skinned_shader;

            // If we swap shaders, we must re-upload the global light uniforms
            if (current_point_shader != target_shader.id)
            {
                GLuint pt_prog = internal->shader_pool[target_shader.id].program;
                glUseProgram(pt_prog);
                current_point_shader = target_shader.id;

                glUniformMatrix4fv(glGetUniformLocation(pt_prog, "u_ShadowMatrices"), 6, GL_FALSE, (float*)shadowTransforms);
                glUniform3fv(glGetUniformLocation(pt_prog, "u_LightPos"), 1, (float*)&pos);
                glUniform1f(glGetUniformLocation(pt_prog, "u_FarPlane"), 100.0f);
            }

            GLuint prog = internal->shader_pool[target_shader.id].program;
            glUniformMatrix4fv(glGetUniformLocation(prog, "u_Model"), 1, GL_FALSE, (float*)&cmd->transform);
            
            GLint bone_loc = glGetUniformLocation(prog, "u_BoneMatrices");
            if (bone_loc != -1)
            {
                if (cmd->bone_matrices != NULL && internal->mesh_pool[cmd->mesh.id].is_skinned)
                {
                    glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);
                }
                else
                {
                    static Matrix4 identity_bones[MAX_BONES];
                    static bool initialized = false;
                    if (!initialized)
                    {
                        for (int b = 0; b < MAX_BONES; b++) identity_bones[b] = Matrix4Identity();
                        initialized = true;
                    }
                    glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)identity_bones);
                }
            }

            glBindVertexArray(internal->mesh_pool[cmd->mesh.id].vao);
            glDrawElements(GL_TRIANGLES, internal->mesh_pool[cmd->mesh.id].index_count, GL_UNSIGNED_INT, 0);
        }
        
        shadow_point_index++;
    }



    glDisable(GL_POLYGON_OFFSET_FILL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    internal->command_count = 0;
}










// Sets OpenGL settings from a given settings struct
static void OpenGL_SetSettings(Renderer* r, const RendererSettings* settings)
{
    if (!r || !r->backend_internal_data || !settings)
        return;
    
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Check if shadow map resolution changed and reallocate if necessary
    if (settings->shadow_map_resolution != internal->state.settings.shadow_map_resolution && settings->shadow_map_resolution > 0)
    {
        uint32_t new_res = settings->shadow_map_resolution;
        if (internal->shadow.depthMapTextureArray != 0)
        {
            glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
            glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, new_res, new_res, MAX_SHADOW_CASCADES, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        }
    }


    internal->state.settings = *settings;
    internal->state.enable_ssao = settings->enable_ssao;

    if (settings->gamma > 0.01f)
        internal->state.gamma = settings->gamma;
    else
        internal->state.gamma = 2.2f;
}





// Returns all the openGL settings in a struct
static RendererSettings OpenGL_GetSettings(Renderer* r)
{
    if (!r || !r->backend_internal_data)
    {
        RendererSettings empty = {0};
        return empty;
    }
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    return internal->state.settings;
}




















// Sets the global camera matrices for the current frame
static void OpenGL_BeginFrame(Renderer* r, const RenderPacket* packet)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    OpenGL_SetViewport(r, 0, 0, packet->window_width, packet->window_height);

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
    internal->state.global_ambient_color = packet->global_ambient_color;
    internal->state.global_ambient_illumination = packet->global_ambient_illumination;
    if (packet->gamma > 0.01f)
        internal->state.gamma = packet->gamma;
    else
        internal->state.gamma = internal->state.settings.gamma > 0.01f ? internal->state.settings.gamma : 2.2f;
    
    if (packet->skybox_shader.id != 0 && packet->skybox_shader.id < MAX_RESOURCES && internal->shader_pool[packet->skybox_shader.id].active)
        internal->skybox.default_shader = packet->skybox_shader;

    // Reset the queue for the new frame
    internal->command_count = 0;
}





// Adds an object to the draw queue
static void OpenGL_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader,
                          TextureHandle albedo, TextureHandle normal, TextureHandle metallic, TextureHandle roughness, TextureHandle ao,
                          MaterialProperties mat_props, Matrix4 transform, Matrix4* bone_matrices,
                          bool is_transparent, float depth_distance, bool cast_shadows, bool receive_shadows)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Return if the queue is full
    if (internal->command_count >= MAX_COMMANDS) return;
    
    internal->command_queue[internal->command_count++] = (RenderCommand){
        mesh,
        shader,
        albedo,
        normal,
        metallic,
        roughness,
        ao,
        mat_props,
        transform,
        bone_matrices,
        is_transparent,
        depth_distance,
        cast_shadows,
        receive_shadows
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
    
    return (int)cmdA->albedo_map.id - (int)cmdB->albedo_map.id;
}





// Uploads generic renderer uniforms
static void OpenGL_UploadCommonUniforms(GLuint program, const RenderState* state)
{
    GLint gamma_loc = glGetUniformLocation(program, "u_Gamma");
    if (gamma_loc != -1)
        glUniform1f(gamma_loc, state->gamma > 0.01f ? state->gamma : 2.2f);

    GLint ambient_color_loc = glGetUniformLocation(program, "u_GlobalAmbientColor");
    if (ambient_color_loc != -1)
        glUniform3fv(ambient_color_loc, 1, (float*)&state->global_ambient_color);

    GLint ambient_int_loc = glGetUniformLocation(program, "u_GlobalAmbientIllumination");
    if (ambient_int_loc != -1)
        glUniform1f(ambient_int_loc, state->global_ambient_illumination);
}





// Extracts the string formatting for lights
static void OpenGL_UploadLightUniforms(GLuint program, const RenderState* state)
{
    OpenGL_UploadCommonUniforms(program, state);
    char uniform_name[64];

    // --- Upload Directional Lights ---
    glUniform1i(glGetUniformLocation(program, "u_DirLightCount"), state->dir_light_count);
    for (uint32_t j = 0; j < state->dir_light_count; j++)
    {
        const DirectionalLightData* dl = &state->dir_lights[j];
        sprintf(uniform_name, "u_DirLights[%d].direction", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&dl->direction);
        sprintf(uniform_name, "u_DirLights[%d].color", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&dl->color);
        sprintf(uniform_name, "u_DirLights[%d].intensity", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), dl->intensity);
        sprintf(uniform_name, "u_DirLights[%d].ambientStrength", j); 
        glUniform1f(glGetUniformLocation(program, uniform_name), dl->ambient_strength);
        sprintf(uniform_name, "u_DirLights[%d].castsShadows", j);
        glUniform1i(glGetUniformLocation(program, uniform_name), dl->casts_shadows ? 1 : 0);
    }


    // --- Upload Point Lights ---
    glUniform1i(glGetUniformLocation(program, "u_PointLightCount"), state->point_light_count);
    for (uint32_t j = 0; j < state->point_light_count; j++)
    {
        const PointLightData* pl = &state->point_lights[j];
        sprintf(uniform_name, "u_PointLights[%d].position", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&pl->position);
        sprintf(uniform_name, "u_PointLights[%d].color", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&pl->color);
        sprintf(uniform_name, "u_PointLights[%d].intensity", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), pl->intensity);
        sprintf(uniform_name, "u_PointLights[%d].constant", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), pl->constant);
        sprintf(uniform_name, "u_PointLights[%d].linear", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), pl->linear);
        sprintf(uniform_name, "u_PointLights[%d].quadratic", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), pl->quadratic);
        sprintf(uniform_name, "u_PointLights[%d].castsShadows", j);
        glUniform1i(glGetUniformLocation(program, uniform_name), pl->casts_shadows ? 1 : 0);
    }


    // --- Upload Spot Lights ---
    glUniform1i(glGetUniformLocation(program, "u_SpotLightCount"), state->spot_light_count);
    for (uint32_t j = 0; j < state->spot_light_count; j++)
    {
        const SpotLightData* sl = &state->spot_lights[j];
        sprintf(uniform_name, "u_SpotLights[%d].position", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&sl->position);
        sprintf(uniform_name, "u_SpotLights[%d].direction", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&sl->direction);
        sprintf(uniform_name, "u_SpotLights[%d].color", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&sl->color);
        sprintf(uniform_name, "u_SpotLights[%d].intensity", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), sl->intensity);
        sprintf(uniform_name, "u_SpotLights[%d].constant", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), sl->constant);
        sprintf(uniform_name, "u_SpotLights[%d].linear", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), sl->linear);
        sprintf(uniform_name, "u_SpotLights[%d].quadratic", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), sl->quadratic);
        sprintf(uniform_name, "u_SpotLights[%d].cutOff", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), sl->inner_cut_off);
        sprintf(uniform_name, "u_SpotLights[%d].outerCutOff", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), sl->outer_cut_off);
        sprintf(uniform_name, "u_SpotLights[%d].castsShadows", j);
        glUniform1i(glGetUniformLocation(program, uniform_name), sl->casts_shadows ? 1 : 0);
    }
}





// Uploads only directional light uniforms
static void OpenGL_UploadDirectionalLightUniforms(GLuint program, const RenderState* state)
{
    char uniform_name[64];

    // --- Upload Directional Lights ---
    glUniform1i(glGetUniformLocation(program, "u_DirLightCount"), state->dir_light_count);
    for (uint32_t j = 0; j < state->dir_light_count; j++)
    {
        const DirectionalLightData* dl = &state->dir_lights[j];
        sprintf(uniform_name, "u_DirLights[%d].direction", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&dl->direction);
        sprintf(uniform_name, "u_DirLights[%d].color", j);
        glUniform3fv(glGetUniformLocation(program, uniform_name), 1, (float*)&dl->color);
        sprintf(uniform_name, "u_DirLights[%d].intensity", j);
        glUniform1f(glGetUniformLocation(program, uniform_name), dl->intensity);
        sprintf(uniform_name, "u_DirLights[%d].ambientStrength", j); 
        glUniform1f(glGetUniformLocation(program, uniform_name), dl->ambient_strength);
        sprintf(uniform_name, "u_DirLights[%d].castsShadows", j);
        glUniform1i(glGetUniformLocation(program, uniform_name), dl->casts_shadows ? 1 : 0);
    }
}





// Executes the geometry pre-pass for SSAO
static void ExecuteGBufferPass(OpenGL_Backend* internal, uint32_t opaque_count)
{
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.gBufferFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    uint32_t current_g_shader = 0;
    uint32_t current_texture = 999999;

    for (uint32_t i = 0; i < opaque_count; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];
        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        ShaderHandle target_g_handle = (cmd->bone_matrices != NULL && internal->mesh_pool[cmd->mesh.id].is_skinned) ? internal->ssao.g_buffer_skinned_shader : internal->ssao.g_buffer_shader;
        GLShader* g_prog = &internal->shader_pool[target_g_handle.id];

        if (current_g_shader != target_g_handle.id)
        {
            glUseProgram(g_prog->program);
            current_g_shader = target_g_handle.id;

            glUniformMatrix4fv(glGetUniformLocation(g_prog->program, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
            glUniformMatrix4fv(glGetUniformLocation(g_prog->program, "u_Projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
            OpenGL_UploadCommonUniforms(g_prog->program, &internal->state);
        }


        // 0. Albedo Map
        glActiveTexture(GL_TEXTURE0);
        bool valid_albedo = (cmd->albedo_map.id != 0 && cmd->albedo_map.id < MAX_RESOURCES);
        glBindTexture(GL_TEXTURE_2D, valid_albedo ? internal->texture_pool[cmd->albedo_map.id].id : internal->texture_pool[1].id); // Fallback to default tex
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.albedoMap"), 0);

        // 1. Normal Map
        bool valid_normal = (cmd->normal_map.id != 0 && cmd->normal_map.id < MAX_RESOURCES && internal->texture_pool[cmd->normal_map.id].active && internal->texture_pool[cmd->normal_map.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasNormalMap"), valid_normal ? 1 : 0);
        glActiveTexture(GL_TEXTURE1);
        if (valid_normal)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->normal_map.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[2].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.normalMap"), 1);

        // 2. Metallic Map
        bool valid_metallic = (cmd->metallic_map.id != 0 && cmd->metallic_map.id < MAX_RESOURCES && internal->texture_pool[cmd->metallic_map.id].active && internal->texture_pool[cmd->metallic_map.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasMetallicMap"), valid_metallic ? 1 : 0);
        glActiveTexture(GL_TEXTURE2);
        if (valid_metallic)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->metallic_map.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[3].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.metallicMap"), 2);

        // 3. Roughness Map
        bool valid_roughness = (cmd->roughness_map.id != 0 && cmd->roughness_map.id < MAX_RESOURCES && internal->texture_pool[cmd->roughness_map.id].active && internal->texture_pool[cmd->roughness_map.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasRoughnessMap"), valid_roughness ? 1 : 0);
        glActiveTexture(GL_TEXTURE3);
        if (valid_roughness)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->roughness_map.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.roughnessMap"), 3);

        // 4. Ambient Occlusion Map
        bool valid_ao = (cmd->ao_map.id != 0 && cmd->ao_map.id < MAX_RESOURCES && internal->texture_pool[cmd->ao_map.id].active && internal->texture_pool[cmd->ao_map.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasAOMap"), valid_ao ? 1 : 0);
        glActiveTexture(GL_TEXTURE4);
        if (valid_ao)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->ao_map.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.aoMap"), 4);
        

        glUniform3fv(glGetUniformLocation(g_prog->program, "u_Material.albedoTint"), 1, (float*)&cmd->mat_props.albedo_tint);
        glUniform1f(glGetUniformLocation(g_prog->program, "u_Material.metallicFactor"), cmd->mat_props.metallic_factor);
        glUniform1f(glGetUniformLocation(g_prog->program, "u_Material.roughnessFactor"), cmd->mat_props.roughness_factor);
        glUniform1f(glGetUniformLocation(g_prog->program, "u_ReceiveShadows"), cmd->receive_shadows ? 1.0f : 0.0f);

        glUniformMatrix4fv(glGetUniformLocation(g_prog->program, "u_Model"), 1, GL_FALSE, (float*)&cmd->transform);

        GLint bone_loc = glGetUniformLocation(g_prog->program, "u_BoneMatrices");
        if (bone_loc != -1)
        {
            if (cmd->bone_matrices != NULL && internal->mesh_pool[cmd->mesh.id].is_skinned)
            {
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);
            }
            else
            {
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

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];
        glBindVertexArray(gl_mesh->vao);
        glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
    }
}





// Executes deferred lighting pass
static void ExecuteDeferredLightingPass(OpenGL_Backend* internal)
{
    // Accumulate every light in linear HDR space. Tone mapping and gamma must happen once, after additive blending
    glBindFramebuffer(GL_FRAMEBUFFER, internal->deferred.lighting_fbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // --- Global pass ---

    GLuint def_prog = internal->shader_pool[internal->deferred.deferred_shader.id].program;
    glUseProgram(def_prog);

    // Bind G-Buffer Textures
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition); glUniform1i(glGetUniformLocation(def_prog, "gPosition"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal); glUniform1i(glGetUniformLocation(def_prog, "gNormal"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, internal->ssao.gAlbedoSpec); glUniform1i(glGetUniformLocation(def_prog, "gAlbedoSpec"), 2);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, internal->state.enable_ssao ? internal->ssao.ssaoColorBufferBlur : internal->ssao.fallbackWhiteTexture); glUniform1i(glGetUniformLocation(def_prog, "ssaoMap"), 3);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray); glUniform1i(glGetUniformLocation(def_prog, "shadowMap"), 4);
    
    // Upload Uniforms
    glUniformMatrix4fv(glGetUniformLocation(def_prog, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
    glUniform3fv(glGetUniformLocation(def_prog, "u_ViewPos"), 1, (float*)&internal->state.camera_pos);
    glUniform1i(glGetUniformLocation(def_prog, "u_EnableSSAO"), internal->state.enable_ssao ? 1 : 0);
    OpenGL_UploadCommonUniforms(def_prog, &internal->state);

    OpenGL_UploadShadowUniforms(def_prog, &internal->state);

    OpenGL_UploadDirectionalLightUniforms(def_prog, &internal->state);

    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(internal->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);



    // --- Local Pass (point light volumes) ---

    GLuint vol_prog = internal->shader_pool[internal->deferred.volume_shader.id].program;
    glUseProgram(vol_prog);

    // Re-bind G-Buffer to the new shader (0, 1, 2)
    glUniform1i(glGetUniformLocation(vol_prog, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(vol_prog, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(vol_prog, "gAlbedoSpec"), 2);

    glUniformMatrix4fv(glGetUniformLocation(vol_prog, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
    glUniformMatrix4fv(glGetUniformLocation(vol_prog, "u_Projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
    glUniform3fv(glGetUniformLocation(vol_prog, "u_ViewPos"), 1, (float*)&internal->state.camera_pos);
    glUniform2f(glGetUniformLocation(vol_prog, "u_ScreenSize"), (float)internal->state.window_width, (float)internal->state.window_height);

    glActiveTexture(GL_TEXTURE6); 
    glUniform1i(glGetUniformLocation(vol_prog, "pointShadowMap"), 6);


    // --- Additive Blending ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glBindVertexArray(internal->deferred.sphere_vao);

    int shadow_point_index = 0;
    for (uint32_t i = 0; i < internal->state.point_light_count; i++)
    {
        PointLightData* pl = &internal->state.point_lights[i];

        if (pl->casts_shadows && shadow_point_index < MAX_SHADOW_CASTING_POINT_LIGHTS)
        {
            glBindTexture(GL_TEXTURE_CUBE_MAP, internal->shadow.pointDepthMaps[shadow_point_index]);
            glUniform1i(glGetUniformLocation(vol_prog, "u_ShadowIndex"), shadow_point_index);
            shadow_point_index++;
        }
        else
        {
            glUniform1i(glGetUniformLocation(vol_prog, "u_ShadowIndex"), -1);
        }
        glUniform1f(glGetUniformLocation(vol_prog, "u_FarPlane"), 100.0f); // Match the projection matrix

        // Calculate physical light radius mathematically based on attenuation
        float max_color = fmaxf(fmaxf(pl->color.r, pl->color.g), pl->color.b) * fmaxf(pl->intensity, 0.0f);
        float radius = 50.0f; // Safe fallback if attenuation is 0
        
        if (pl->quadratic > 0.0001f)
        {
            float discriminant = pl->linear * pl->linear - 4 * pl->quadratic * (pl->constant - (256.0f / 5.0f) * max_color);
            radius = 0.0f;
            if (discriminant > 0.0f)
                radius = (-pl->linear + sqrtf(discriminant)) / (2.0f * pl->quadratic);
        }
        else if (pl->linear > 0.0001f)
            radius = ((256.0f / 5.0f) * max_color - pl->constant) / pl->linear;

        if (radius <= 0.0f)
            continue;

        float volume_radius = radius * 1.2f;

        // Transform the Sphere (Scale -> Translate)
        Matrix4 model = Matrix4Translate(pl->position);
        model = Matrix4Multiply(model, Matrix4Scale((Vector3){volume_radius, volume_radius, volume_radius}));

        glUniformMatrix4fv(glGetUniformLocation(vol_prog, "u_Model"), 1, GL_FALSE, (float*)&model);

        // Upload exactly one light's data
        glUniform3fv(glGetUniformLocation(vol_prog, "u_LightPos"), 1, (float*)&pl->position);
        glUniform3fv(glGetUniformLocation(vol_prog, "u_LightColor"), 1, (float*)&pl->color);
        glUniform1f(glGetUniformLocation(vol_prog, "u_Intensity"), pl->intensity);
        glUniform1f(glGetUniformLocation(vol_prog, "u_Constant"), pl->constant);
        glUniform1f(glGetUniformLocation(vol_prog, "u_Linear"), pl->linear);
        glUniform1f(glGetUniformLocation(vol_prog, "u_Quadratic"), pl->quadratic);
        glUniform1f(glGetUniformLocation(vol_prog, "u_Radius"), radius);

        // Draw Sphere
        glDrawElements(GL_TRIANGLES, internal->deferred.sphere_index_count, GL_UNSIGNED_INT, 0);
    }



    // --- Local Pass (spot light volumes) ---

    GLuint spot_prog = internal->shader_pool[internal->deferred.spot_volume_shader.id].program;
    glUseProgram(spot_prog);

    glUniform1i(glGetUniformLocation(spot_prog, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(spot_prog, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(spot_prog, "gAlbedoSpec"), 2);

    // Bind the spotlight shadow map array to texture unit 5
    glActiveTexture(GL_TEXTURE5); 
    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.spotDepthMapTextureArray); 
    glUniform1i(glGetUniformLocation(spot_prog, "spotShadowMap"), 5);
    
    glUniformMatrix4fv(glGetUniformLocation(spot_prog, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
    glUniformMatrix4fv(glGetUniformLocation(spot_prog, "u_Projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
    glUniform3fv(glGetUniformLocation(spot_prog, "u_ViewPos"), 1, (float*)&internal->state.camera_pos);
    glUniform2f(glGetUniformLocation(spot_prog, "u_ScreenSize"), (float)internal->state.window_width, (float)internal->state.window_height);


    int shadow_spot_index = 0;
    for (uint32_t i = 0; i < internal->state.spot_light_count; i++)
    {
        SpotLightData* sl = &internal->state.spot_lights[i];

        if (sl->casts_shadows && shadow_spot_index < MAX_SHADOW_CASTING_SPOTLIGHTS)
        {
            // Upload the specific light-space matrix for this spotlight
            glUniformMatrix4fv(glGetUniformLocation(spot_prog, "u_LightSpaceMatrix"), 1, GL_FALSE, (float*)&internal->state.spot_light_matrices[shadow_spot_index]);
            glUniform1i(glGetUniformLocation(spot_prog, "u_ShadowIndex"), shadow_spot_index);
            shadow_spot_index++;
        }
        else
        {
            glUniform1i(glGetUniformLocation(spot_prog, "u_ShadowIndex"), -1);
        }

        // Same physical radius calculation so the sphere encompasses the cone's reach
        float max_color = fmaxf(fmaxf(sl->color.r, sl->color.g), sl->color.b) * fmaxf(sl->intensity, 0.0f);
        float radius = 50.0f; // Safe fallback
        
        if (sl->quadratic > 0.0001f)
        {
            float discriminant = sl->linear * sl->linear - 4 * sl->quadratic * (sl->constant - (256.0f / 5.0f) * max_color);
            radius = 0.0f;
            if (discriminant > 0.0f)
                radius = (-sl->linear + sqrtf(discriminant)) / (2.0f * sl->quadratic);
        }
        else if (sl->linear > 0.0001f)
            radius = ((256.0f / 5.0f) * max_color - sl->constant) / sl->linear;

        if (radius <= 0.0f)
            continue;

        float volume_radius = radius * 1.2f;

        Matrix4 model = Matrix4Translate(sl->position);
        model = Matrix4Multiply(model, Matrix4Scale((Vector3){volume_radius, volume_radius, volume_radius}));

        glUniformMatrix4fv(glGetUniformLocation(spot_prog, "u_Model"), 1, GL_FALSE, (float*)&model);

        glUniform3fv(glGetUniformLocation(spot_prog, "u_LightPos"), 1, (float*)&sl->position);
        glUniform3fv(glGetUniformLocation(spot_prog, "u_LightDir"), 1, (float*)&sl->direction);
        glUniform3fv(glGetUniformLocation(spot_prog, "u_LightColor"), 1, (float*)&sl->color);
        glUniform1f(glGetUniformLocation(spot_prog, "u_Intensity"), sl->intensity);
        glUniform1f(glGetUniformLocation(spot_prog, "u_Constant"), sl->constant);
        glUniform1f(glGetUniformLocation(spot_prog, "u_Linear"), sl->linear);
        glUniform1f(glGetUniformLocation(spot_prog, "u_Quadratic"), sl->quadratic);
        glUniform1f(glGetUniformLocation(spot_prog, "u_Radius"), radius);
        glUniform1f(glGetUniformLocation(spot_prog, "u_CutOff"), sl->inner_cut_off);
        glUniform1f(glGetUniformLocation(spot_prog, "u_OuterCutOff"), sl->outer_cut_off);

        glDrawElements(GL_TRIANGLES, internal->deferred.sphere_index_count, GL_UNSIGNED_INT, 0);
    }


    // --- Restore pipeline defaults ---
    glDisable(GL_BLEND);
    glCullFace(GL_BACK);

    // Convert the completed linear lighting buffer to the display color space.
    OpenGL_BindDefaultFramebuffer();

    GLuint post_prog = internal->shader_pool[internal->deferred.post_shader.id].program;
    glUseProgram(post_prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, internal->deferred.lighting_texture);
    glUniform1i(glGetUniformLocation(post_prog, "hdrLightingMap"), 0);
    OpenGL_UploadCommonUniforms(post_prog, &internal->state);

    glBindVertexArray(internal->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glEnable(GL_DEPTH_TEST);
}





// Computes and blurs the SSAO texture
static void ExecuteSSAOPass(OpenGL_Backend* internal)
{
    glViewport(0, 0, internal->state.window_width / 2, internal->state.window_height / 2);

    // --- Compute Pass ---
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    GLuint ssao_prog = internal->shader_pool[internal->ssao.ssao_shader.id].program;
    glUseProgram(ssao_prog);

    glUniformMatrix4fv(glGetUniformLocation(ssao_prog, "projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
    glUniformMatrix4fv(glGetUniformLocation(ssao_prog, "view"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
    glUniform1i(glGetUniformLocation(ssao_prog, "kernelSize"), 16);
    glUniform1f(glGetUniformLocation(ssao_prog, "radius"), 0.5f);
    glUniform1f(glGetUniformLocation(ssao_prog, "bias"), 0.025f);

    for (int k = 0; k < 64; ++k)
    {
        char var_name[32];
        sprintf(var_name, "samples[%d]", k);
        glUniform3fv(glGetUniformLocation(ssao_prog, var_name), 1, (float*)&internal->ssao.kernel[k]);
    }

    glUniform2f(glGetUniformLocation(ssao_prog, "noiseScale"), (float)internal->state.window_width / 4.0f, (float)internal->state.window_height / 4.0f);

    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition); glUniform1i(glGetUniformLocation(ssao_prog, "gPosition"), 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal); glUniform1i(glGetUniformLocation(ssao_prog, "gNormal"), 1);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, internal->ssao.noiseTexture); glUniform1i(glGetUniformLocation(ssao_prog, "texNoise"), 2);

    glBindVertexArray(internal->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- Blur Pass ---
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
}





// Executes a forward rendering loop (used for both Opaque and Transparent batches)
static void OpenGL_RenderCommandBatch(OpenGL_Backend* internal, uint32_t start_idx, uint32_t end_idx)
{
    uint32_t current_shader = 0;
    uint32_t current_texture = 0;

    for (uint32_t i = start_idx; i < end_idx; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];
        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        ShaderHandle target_handle = cmd->shader;
        if (target_handle.id == 0)
            target_handle = (cmd->bone_matrices != NULL && internal->mesh_pool[cmd->mesh.id].is_skinned) ? internal->forward.animated_shader : internal->forward.default_shader;

        GLShader* gl_shader = &internal->shader_pool[target_handle.id];
        if (!gl_shader->active)
            continue;

        if (current_shader != target_handle.id)
        {
            glUseProgram(gl_shader->program);
            current_shader = target_handle.id;

            glUniformMatrix4fv(glGetUniformLocation(gl_shader->program, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
            glUniformMatrix4fv(glGetUniformLocation(gl_shader->program, "u_Projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
            glUniform3fv(glGetUniformLocation(gl_shader->program, "u_ViewPos"), 1, (float*)&internal->state.camera_pos);

            OpenGL_UploadShadowUniforms(gl_shader->program, &internal->state);
            OpenGL_BindSSAOTexture(internal, gl_shader->program);

            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
            glUniform1i(glGetUniformLocation(gl_shader->program, "shadowMap"), 6);

            GLint enable_ssao_loc = glGetUniformLocation(gl_shader->program, "u_EnableSSAO");
            if (enable_ssao_loc != -1)
                glUniform1i(enable_ssao_loc, internal->state.enable_ssao ? 1 : 0);

            OpenGL_UploadLightUniforms(gl_shader->program, &internal->state);
        }


        // 0. Albedo Map
        glActiveTexture(GL_TEXTURE0);
        bool valid_albedo = (cmd->albedo_map.id != 0 && cmd->albedo_map.id < MAX_RESOURCES && internal->texture_pool[cmd->albedo_map.id].active && internal->texture_pool[cmd->albedo_map.id].id != 0);
        glBindTexture(GL_TEXTURE_2D, valid_albedo ? internal->texture_pool[cmd->albedo_map.id].id : internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.albedoMap"), 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.diffuse"), 0);

        // 1. Normal Map
        bool valid_normal = (cmd->normal_map.id != 0 && cmd->normal_map.id < MAX_RESOURCES && internal->texture_pool[cmd->normal_map.id].active && internal->texture_pool[cmd->normal_map.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasNormalMap"), valid_normal ? 1 : 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, valid_normal ? internal->texture_pool[cmd->normal_map.id].id : internal->texture_pool[2].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.normalMap"), 1);

        // 2. Metallic Map
        bool valid_metallic = (cmd->metallic_map.id != 0 && cmd->metallic_map.id < MAX_RESOURCES && internal->texture_pool[cmd->metallic_map.id].active && internal->texture_pool[cmd->metallic_map.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasMetallicMap"), valid_metallic ? 1 : 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, valid_metallic ? internal->texture_pool[cmd->metallic_map.id].id : internal->texture_pool[3].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.metallicMap"), 2);

        // 3. Roughness Map
        bool valid_roughness = (cmd->roughness_map.id != 0 && cmd->roughness_map.id < MAX_RESOURCES && internal->texture_pool[cmd->roughness_map.id].active && internal->texture_pool[cmd->roughness_map.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasRoughnessMap"), valid_roughness ? 1 : 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, valid_roughness ? internal->texture_pool[cmd->roughness_map.id].id : internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.roughnessMap"), 3);

        // 4. Ambient Occlusion Map
        bool valid_ao = (cmd->ao_map.id != 0 && cmd->ao_map.id < MAX_RESOURCES && internal->texture_pool[cmd->ao_map.id].active && internal->texture_pool[cmd->ao_map.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasAOMap"), valid_ao ? 1 : 0);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, valid_ao ? internal->texture_pool[cmd->ao_map.id].id : internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.aoMap"), 4);


        glUniformMatrix4fv(glGetUniformLocation(gl_shader->program, "u_Model"), 1, GL_FALSE, (float*)&cmd->transform);
        glUniform3fv(glGetUniformLocation(gl_shader->program, "u_Material.tint"), 1, (float*)&cmd->mat_props.albedo_tint);
        glUniform1f(glGetUniformLocation(gl_shader->program, "u_Material.metallicFactor"), cmd->mat_props.metallic_factor);
        glUniform1f(glGetUniformLocation(gl_shader->program, "u_Material.roughnessFactor"), cmd->mat_props.roughness_factor);
        glUniform1f(glGetUniformLocation(gl_shader->program, "u_ReceiveShadows"), cmd->receive_shadows ? 1.0f : 0.0f);

        GLint bone_loc = glGetUniformLocation(gl_shader->program, "u_BoneMatrices");
        if (bone_loc != -1)
        {
            // if (cmd->bone_matrices != NULL)
            if (cmd->bone_matrices != NULL && internal->mesh_pool[cmd->mesh.id].is_skinned)
            {
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)cmd->bone_matrices);
            }
            else
            {
                static Matrix4 identity_bones[MAX_BONES];
                static bool initialized = false;
                if (!initialized)
                {
                    for (int b = 0; b < MAX_BONES; b++) identity_bones[b] = Matrix4Identity();
                    initialized = true;
                }
                glUniformMatrix4fv(bone_loc, MAX_BONES, GL_FALSE, (float*)identity_bones);
            }
        }

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];
        glBindVertexArray(gl_mesh->vao);
        glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
    }
}





// Renders the Skybox
static void OpenGL_DrawSkybox(OpenGL_Backend* internal)
{
    uint32_t shader_id = internal->skybox.default_shader.id;
    uint32_t tex_id = internal->state.skybox_texture.id;

    // Validate both handles so a stale ID can't bind a program.
    if (shader_id >= MAX_RESOURCES || !internal->shader_pool[shader_id].active)
        return;
    if (tex_id >= MAX_RESOURCES || !internal->texture_pool[tex_id].active)
        return;

    if (shader_id != 0 && tex_id != 0)
    {
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_CULL_FACE);

        GLuint prog = internal->shader_pool[shader_id].program;
        glUseProgram(prog);
        
        glUniformMatrix4fv(glGetUniformLocation(prog, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
        glUniformMatrix4fv(glGetUniformLocation(prog, "u_Projection"), 1, GL_FALSE, (float*)&internal->state.projection_matrix);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[internal->state.skybox_texture.id].id);

        GLint skybox_loc = glGetUniformLocation(prog, "u_Skybox");
        if (skybox_loc != -1)
            glUniform1i(skybox_loc, 0);
        
        glBindVertexArray(internal->skybox.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36); 
        glBindVertexArray(0);
        
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE);
    }
}





// Sorts the queue, binds the state, and executes the actual GPU draw calls
static void OpenGL_EndFrame(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    glViewport(0, 0, internal->state.window_width, internal->state.window_height);

    // Sort the command queue
    qsort(internal->command_queue, internal->command_count, sizeof(RenderCommand), CompareRenderCommands);

    // Find where the transparent commands begin
    uint32_t transparent_start_idx = internal->command_count;
    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        if (internal->command_queue[i].is_transparent)
        {
            transparent_start_idx = i;
            break;
        }
    }



    // --- Deferred pipeline ---

    glEnable(GL_CULL_FACE);

    // Generate the G_Buffer for opaque objects
    ExecuteGBufferPass(internal, transparent_start_idx);
    
    // Calculate SSAO if enabled and shaders exist
    if (internal->state.enable_ssao && internal->ssao.g_buffer_shader.id != 0 &&
        internal->ssao.ssao_shader.id != 0 && internal->ssao.blur_shader.id != 0)
    {
        ExecuteSSAOPass(internal);
    }

    glViewport(0, 0, internal->state.window_width, internal->state.window_height);

    // Deferred lighting pass
    ExecuteDeferredLightingPass(internal);



    // --- Forward pipeline (For Skybox and Transparent Geometry) ---

    // Depth Blit. We must copy the exact depths from the G-Buffer onto the main screen 
    // so the Skybox and Transparent objects know what to hide behind
    glBindFramebuffer(GL_READ_FRAMEBUFFER, internal->ssao.gBufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, internal->state.window_width, internal->state.window_height,
                      0, 0, internal->state.window_width, internal->state.window_height,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    OpenGL_BindDefaultFramebuffer();

    // Draw Skybox
    if (internal->state.has_skybox)
        OpenGL_DrawSkybox(internal);

    // Draw transparents using the forward renderer
    if (transparent_start_idx < internal->command_count)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE); // Protect depth buffer from transparent overlap
        glDisable(GL_CULL_FACE);

        OpenGL_RenderCommandBatch(internal, transparent_start_idx, internal->command_count);

        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE); // Restore depth writing
        glEnable(GL_CULL_FACE);
    }

    glBindVertexArray(0);
}





// Generates a simple low-poly UV sphere for light volumes
static void OpenGL_GenerateLightSphere(OpenGL_Backend* internal)
{
    const int rings = 32;
    const int sectors = 32;
    const float PI = 3.14159265359f;

    float vertices[32 * 32 * 3];
    uint32_t indices[32 * 32 * 6];
    
    int v = 0;
    for (int r = 0; r < rings; ++r)
    {
        for (int s = 0; s < sectors; ++s)
        {
            float const y = sin(-PI/2.0f + PI * r / (float)(rings-1));
            float const x = cos(2*PI * s / (float)(sectors-1)) * sin(PI * r / (float)(rings-1));
            float const z = sin(2*PI * s / (float)(sectors-1)) * sin(PI * r / (float)(rings-1));

            vertices[v++] = x; vertices[v++] = y; vertices[v++] = z;
        }
    }

    int i = 0;

    for (int r = 0; r < rings - 1; ++r)
    {
        for (int s = 0; s < sectors - 1; ++s)
        {
            indices[i++] = r * sectors + s;
            indices[i++] = r * sectors + (s+1);
            indices[i++] = (r+1) * sectors + (s+1);
            indices[i++] = r * sectors + s;
            indices[i++] = (r+1) * sectors + (s+1);
            indices[i++] = (r+1) * sectors + s;
        }
    }

    internal->deferred.sphere_index_count = i;

    glGenVertexArrays(1, &internal->deferred.sphere_vao);
    glGenBuffers(1, &internal->deferred.sphere_vbo);
    glGenBuffers(1, &internal->deferred.sphere_ebo);

    glBindVertexArray(internal->deferred.sphere_vao);
    glBindBuffer(GL_ARRAY_BUFFER, internal->deferred.sphere_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, internal->deferred.sphere_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
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

    // Generate Guaranteed Default 1x1 Textures for Fallbacks & Safe Binding
    unsigned char white_pixel[4]  = { 255, 255, 255, 255 };
    unsigned char normal_pixel[4] = { 128, 128, 255, 255 };
    unsigned char black_pixel[4]  = { 0,   0,   0,   255 };

    glGenTextures(1, &internal->default_white_texture);
    glBindTexture(GL_TEXTURE_2D, internal->default_white_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &internal->default_normal_texture);
    glBindTexture(GL_TEXTURE_2D, internal->default_normal_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, normal_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenTextures(1, &internal->default_black_texture);
    glBindTexture(GL_TEXTURE_2D, internal->default_black_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black_pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Reserve slots 1, 2, and 3 inside texture_pool exclusively for these default textures
    internal->texture_pool[1].id = internal->default_white_texture;
    internal->texture_pool[1].active = true;

    internal->texture_pool[2].id = internal->default_normal_texture;
    internal->texture_pool[2].active = true;

    internal->texture_pool[3].id = internal->default_black_texture;
    internal->texture_pool[3].active = true;


    
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



    // Generate depth map buffers for directional lights

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



    // Generate spotlight depth map buffers
    
    glGenFramebuffers(1, &internal->shadow.spotDepthMapFBO);
    glGenTextures(1, &internal->shadow.spotDepthMapTextureArray);

    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.spotDepthMapTextureArray);
    // Spotlights cover a much smaller area, so a size of 1024 is enough.
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, 1024, 1024, MAX_SHADOW_CASTING_SPOTLIGHTS, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.spotDepthMapFBO);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, internal->shadow.spotDepthMapTextureArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



    // --- Generate G-Buffer ---

    uint32_t win_w = init_width;
    uint32_t win_h = init_height;

    glGenFramebuffers(1, &internal->ssao.gBufferFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ssao.gBufferFBO);

    // Position color buffer (use RGBA16F for GPU alignment)
    glGenTextures(1, &internal->ssao.gPosition);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->ssao.gPosition, 0);

    // Normal color buffer
    glGenTextures(1, &internal->ssao.gNormal);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, internal->ssao.gNormal, 0);

    // Albedo + Specular color buffer
    glGenTextures(1, &internal->ssao.gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, internal->ssao.gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, win_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, internal->ssao.gAlbedoSpec, 0);

    // Tell OpenGL we now have THREE color attachments
    uint32_t attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, attachments);

    // Create and attach depth buffer (renderbuffer)
    glGenRenderbuffers(1, &internal->ssao.gDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ssao.gDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, win_w, win_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, internal->ssao.gDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log_Error("G-Buffer Framebuffer not complete!");

    // --- Generate linear HDR lighting target ---
    glGenFramebuffers(1, &internal->deferred.lighting_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->deferred.lighting_fbo);

    glGenTextures(1, &internal->deferred.lighting_texture);
    glBindTexture(GL_TEXTURE_2D, internal->deferred.lighting_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, win_w, win_h, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal->deferred.lighting_texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        Log_Error("Deferred lighting framebuffer not complete!");

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


    // Generate light spheres
    OpenGL_GenerateLightSphere(internal);


    // Generate Point Light Cubemap Array    
    glGenFramebuffers(1, &internal->shadow.pointDepthMapFBO);
    
    for (int i = 0; i < MAX_SHADOW_CASTING_POINT_LIGHTS; i++)
    {
        glGenTextures(1, &internal->shadow.pointDepthMaps[i]);
        glBindTexture(GL_TEXTURE_CUBE_MAP, internal->shadow.pointDepthMaps[i]);
        
        // Allocate the 6 faces individually
        for (unsigned int face = 0; face < 6; ++face)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_DEPTH_COMPONENT24, 
                         1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        }
        
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.pointDepthMapFBO);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);



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

    r->SetSettings = OpenGL_SetSettings;
    r->GetSettings = OpenGL_GetSettings;
    
    return r;
}