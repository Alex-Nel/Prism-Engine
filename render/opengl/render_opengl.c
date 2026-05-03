#include "../../include/glad/glad.h"
#include "../render.h"
#include "../../core/log.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>



// Struct for a global render state
typedef struct RenderState
{
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;
    DirectionalLight global_light;
    PointLightData point_lights[MAX_RESOURCES];
    uint32_t point_light_count;
} RenderState;


// Global render state to be used by OpenGL
// static RenderState state;





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





// Internal data pools for meshes shaders and textures
// static GLMesh mesh_pool[MAX_RESOURCES];
// static GLShader shader_pool[MAX_RESOURCES];
// static GLTexture texture_pool[MAX_RESOURCES];





// Struct for a render command. Contains mesh, shader, texture, material, and transform data
typedef struct RenderCommand
{
    MeshHandle mesh;
    ShaderHandle shader;
    TextureHandle texture;
    MaterialProperties mat_props;
    Matrix4 transform;
} RenderCommand;



// Internal data pool for given command queues
// static RenderCommand command_queue[MAX_COMMANDS];
// static uint32_t command_count = 0;




typedef struct OpenGL_Backend
{
    GLMesh mesh_pool[MAX_RESOURCES];
    GLShader shader_pool[MAX_RESOURCES];
    GLTexture texture_pool[MAX_RESOURCES];

    RenderCommand command_queue[MAX_COMMANDS];
    uint32_t command_count;

    RenderState state;
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
        // Optional: You could add a LOG_WARN here to tell the user they 
        // had a memory leak during runtime!

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
    glViewport(x, y, width, height);
}

// Sets the color of the renderer to clear with
static void OpenGL_SetClearColor(Renderer* renderer, float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

// Clears the renderer
static void OpenGL_Clear(Renderer* r)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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















// Sets the global camera matrices for the current frame
static void OpenGL_BeginFrame(Renderer* r, const RenderPacket* packet)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    internal->state.view_matrix = packet->view_matrix;
    internal->state.projection_matrix = packet->projection_matrix;
    internal->state.camera_pos = packet->camera_pos;
    internal->state.global_light = packet->global_light;

    internal->state.point_light_count = packet->point_light_count;
    for (uint32_t i = 0; i < packet->point_light_count; i++)
        internal->state.point_lights[i] = packet->point_lights[i];
    
    // Reset the queue for the new frame
    internal->command_count = 0;
}



// Adds an object to the draw queue
static void OpenGL_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    // Return if the queue is full
    if (internal->command_count >= MAX_COMMANDS) return;
    
    internal->command_queue[internal->command_count++] = (RenderCommand){
        mesh,
        shader,
        texture,
        mat_props,
        transform
    };
}



// Compare render commands (for sorting)
static int CompareRenderCommands(const void* a, const void* b)
{
    RenderCommand* cmdA = (RenderCommand*)a;
    RenderCommand* cmdB = (RenderCommand*)b;

    // Sort primarily by shader, then by texture
    if (cmdA->shader.id != cmdB->shader.id)
        return (int)cmdA->shader.id - (int)cmdB->shader.id;
    
    return (int)cmdA->texture.id - (int)cmdB->texture.id;
}



// Sorts the queue, binds the state, and executes the actual GPU draw calls
static void OpenGL_EndFrame(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Sort the command queue
    qsort(internal->command_queue, internal->command_count, sizeof(RenderCommand), CompareRenderCommands);

    // Track current state to avoid redundant binds
    uint32_t current_shader = 0;
    uint32_t current_texture = 0;

    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        RenderCommand* cmd = &internal->command_queue[i];

        if (!internal->mesh_pool[cmd->mesh.id].active || !internal->shader_pool[cmd->shader.id].active) continue;

        GLMesh* gl_mesh = &internal->mesh_pool[cmd->mesh.id];
        GLShader* gl_shader = &internal->shader_pool[cmd->shader.id];

        // Bind Shader
        glUseProgram(gl_shader->program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->texture.id].id);

        // Upload Matrices
        GLint model_loc = glGetUniformLocation(gl_shader->program, "u_Model");
        GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
        GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
        
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&internal->state.view_matrix);
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&internal->state.projection_matrix);

        // Upload Lighting Data
        GLint view_pos_loc  = glGetUniformLocation(gl_shader->program, "u_ViewPos");
        GLint light_pos_loc = glGetUniformLocation(gl_shader->program, "u_LightDir");
        GLint light_col_loc = glGetUniformLocation(gl_shader->program, "u_LightColor");
        GLint ambient_loc   = glGetUniformLocation(gl_shader->program, "u_AmbientStrength");

        if (view_pos_loc != -1)  glUniform3fv(view_pos_loc, 1, (float*)&internal->state.camera_pos);
        if (light_pos_loc != -1) glUniform3fv(light_pos_loc, 1, (float*)&internal->state.global_light.direction);
        if (light_col_loc != -1) glUniform3fv(light_col_loc, 1, (float*)&internal->state.global_light.color);
        if (ambient_loc != -1)   glUniform1f(ambient_loc, internal->state.global_light.ambient_strength);


        glUniform1i(glGetUniformLocation(gl_shader->program, "u_PointLightCount"), internal->state.point_light_count);

        char uniform_name[64];
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


        GLint diff_loc = glGetUniformLocation(gl_shader->program, "u_Material.diffuse");
        if (diff_loc != -1) glUniform1i(diff_loc, 0);

        GLint tint_loc = glGetUniformLocation(gl_shader->program, "u_Material.tint");
        if (tint_loc != -1) glUniform3fv(tint_loc, 1, (float*)&cmd->mat_props.tint_color);

        GLint shine_loc = glGetUniformLocation(gl_shader->program, "u_Material.shininess");
        if (shine_loc != -1) glUniform1f(shine_loc, cmd->mat_props.shininess);

        GLint spec_loc = glGetUniformLocation(gl_shader->program, "u_Material.specularStrength");
        if (spec_loc != -1) glUniform1f(spec_loc, cmd->mat_props.specular_strength);
        

        // Draw
        glBindVertexArray(gl_mesh->vao);
        glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}





// OpenGL Initialization function
Renderer* Render_Init(Render_LoadProcFn load_proc)
{
    Renderer* r = malloc(sizeof(Renderer));
    if (!r) return NULL;

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



    r->backend_internal_data = internal;

    r->api = GRAPHICS_API_OPENGL;
    r->Shutdown = OpenGL_Shutdown;
    r->SetViewport = OpenGL_SetViewport;
    r->SetClearColor = OpenGL_SetClearColor;
    r->Clear = OpenGL_Clear;

    r->CreateMesh = OpenGL_CreateMesh;
    r->DestroyMesh = OpenGL_DestroyMesh;

    r->CreateTexture = OpenGL_CreateTexture;
    r->DestroyTexture = OpenGL_DestroyTexture;

    r->CreateShader = OpenGL_CreateShader;
    r->DestroyShader = OpenGL_DestroyShader;

    r->BeginFrame = OpenGL_BeginFrame;
    r->Submit = OpenGL_Submit;
    r->EndFrame = OpenGL_EndFrame;
    
    return r;
}