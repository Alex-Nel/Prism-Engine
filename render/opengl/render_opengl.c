#include "../../include/glad/glad.h"
#include "../render.h"
#include "../../core/log.h"

#include <stddef.h>



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
static RenderState state;





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
static GLMesh mesh_pool[MAX_RESOURCES];
static GLShader shader_pool[MAX_RESOURCES];
static GLTexture texture_pool[MAX_RESOURCES];





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
static RenderCommand command_queue[MAX_COMMANDS];
static uint32_t command_count = 0;





// OpenGL Initialization function
bool Render_Init(GraphicsAPI api, Render_LoadProcFn load_proc)
{
    // Initialize data pools
    for (int i = 0; i < MAX_RESOURCES; i++)
    {
        mesh_pool[i].active = false;
        shader_pool[i].active = false;
        texture_pool[i].active = false;
    }


    // Select correct graphics backend
    if (api == GRAPHICS_API_OPENGL)
    {
        // Load OpenGL functions using the provided loader
        if (!gladLoadGLLoader((GLADloadproc)load_proc))
        {
            Log_Error("Failed to initialize OpenGL loader!");
            return false;
        }

        // Set global OpenGL state
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE); // Disables drawing the inside of a mesh
        
        return true;
    }
    else if (api == GRAPHICS_API_VULKAN)
    {
        Log_Error("Vulkan not implemented yet\n");
        return false;
    }
    else if (api == GRAPHICS_API_DIRECTX)
    {
        Log_Error("DirectX not implemented yet\n");
        return false;
    }

    Log_Error("Unkown Graphics API\n");
    return true;
}





// Shutdown function for OpenGL
void Render_Shutdown(void)
{
    // Clear out any pending draw commands
    command_count = 0;

    // Garbage Collector Loop. We start at 1 because index 0 is the "Invalid/Null" handle.
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {    
        // Optional: You could add a LOG_WARN here to tell the user they 
        // had a memory leak during runtime!

        if (mesh_pool[i].active)
            Render_DestroyMesh((MeshHandle){i});
        
        if (texture_pool[i].active)
            Render_DestroyTexture((TextureHandle){i});
        
        if (shader_pool[i].active)
            Render_DestroyShader((ShaderHandle){i});
    }
}










// Sets the size and position of the viewport
void Render_SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    glViewport(x, y, width, height);
}

// Sets the color of the renderer to clear with
void Render_SetClearColor(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

// Clears the renderer
void Render_Clear(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}















// Uploads vertex and index data to the GPU and returns a handle
MeshHandle Render_CreateMesh(const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count)
{
    // Find an empty slot
    // TODO: use a free-list for O(1) allocation
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!mesh_pool[i].active)
        {
            id = i;
            break;
        }
    }

    // Return 0 if pool is full
    if (id == 0) return (MeshHandle){0};

    GLMesh* mesh = &mesh_pool[id];
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
TextureHandle Render_CreateTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    if (!pixels) return (TextureHandle){0};

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
        if (!texture_pool[i].active)
        {    
            // Store the raw OpenGL ID inside the pool
            texture_pool[i].id = texture_id;
            texture_pool[i].active = true;
            
            // Return the pool index
            return (TextureHandle){ i }; 
        }
    }

    // Fall back if no more texture slots
    glDeleteTextures(1, &texture_id);

    return (TextureHandle){ 0 };
}





// Uploads vertex and fragment shaders to the GPU to make a complete shader. Returns a handle
ShaderHandle Render_CreateShader(const char* vertex_source, const char* fragment_source)
{
    // Check if another spot in the pool is available
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!shader_pool[i].active)
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
    GLShader* shader = &shader_pool[id];
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
void Render_DestroyMesh(MeshHandle mesh)
{
    // Validate the handle to prevent segfaults
    if (mesh.id == 0 || mesh.id >= MAX_RESOURCES)
        return;

    GLMesh* gl_mesh = &mesh_pool[mesh.id];
    
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
void Render_DestroyTexture(TextureHandle texture)
{
    // Validate the handle to prevent segfaults
    if (texture.id == 0 || texture.id >= MAX_RESOURCES)
        return;

    GLTexture* tex = &texture_pool[texture.id];

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
void Render_DestroyShader(ShaderHandle shader)
{
    // Validate the handle to prevent segfaults
    if (shader.id == 0 || shader.id >= MAX_RESOURCES)
        return;

    GLShader* gl_shader = &shader_pool[shader.id];

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
void Render_BeginFrame(const RenderPacket* packet)
{
    state.view_matrix = packet->view_matrix;
    state.projection_matrix = packet->projection_matrix;
    state.camera_pos = packet->camera_pos;
    state.global_light = packet->global_light;

    state.point_light_count = packet->point_light_count;
    for (uint32_t i = 0; i < packet->point_light_count; i++)
        state.point_lights[i] = packet->point_lights[i];
    
    // Reset the queue for the new frame
    command_count = 0;
}



// Adds an object to the draw queue
void Render_Submit(MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform)
{
    // Return if the queue is full
    if (command_count >= MAX_COMMANDS) return;
    
    command_queue[command_count++] = (RenderCommand){
        mesh,
        shader,
        texture,
        mat_props,
        transform
    };
}



// Sorts the queue, binds the state, and executes the actual GPU draw calls
void Render_EndFrame()
{
    // TODO: Sort the command_queue by ShaderHandle to minimize OpenGL state changes
    // Iterate through command queue
    for (uint32_t i = 0; i < command_count; i++)
    {
        RenderCommand* cmd = &command_queue[i];

        if (!mesh_pool[cmd->mesh.id].active || !shader_pool[cmd->shader.id].active) continue;

        GLMesh* gl_mesh = &mesh_pool[cmd->mesh.id];
        GLShader* gl_shader = &shader_pool[cmd->shader.id];

        // Bind Shader
        glUseProgram(gl_shader->program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_pool[cmd->texture.id].id);

        // Upload Matrices
        GLint model_loc = glGetUniformLocation(gl_shader->program, "u_Model");
        GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
        GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
        
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&state.view_matrix);
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&state.projection_matrix);

        // Upload Lighting Data
        GLint view_pos_loc  = glGetUniformLocation(gl_shader->program, "u_ViewPos");
        GLint light_pos_loc = glGetUniformLocation(gl_shader->program, "u_LightDir");
        GLint light_col_loc = glGetUniformLocation(gl_shader->program, "u_LightColor");
        GLint ambient_loc   = glGetUniformLocation(gl_shader->program, "u_AmbientStrength");

        if (view_pos_loc != -1)  glUniform3fv(view_pos_loc, 1, (float*)&state.camera_pos);
        if (light_pos_loc != -1) glUniform3fv(light_pos_loc, 1, (float*)&state.global_light.direction);
        if (light_col_loc != -1) glUniform3fv(light_col_loc, 1, (float*)&state.global_light.color);
        if (ambient_loc != -1)   glUniform1f(ambient_loc, state.global_light.ambient_strength);


        glUniform1i(glGetUniformLocation(gl_shader->program, "u_PointLightCount"), state.point_light_count);

        char uniform_name[64];
        for (uint32_t j = 0; j < state.point_light_count; j++)
        {
            PointLightData* pl = &state.point_lights[j];
            
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