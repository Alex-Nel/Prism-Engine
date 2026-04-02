#include "../../include/glad/glad.h"
#include "../render.h"
#include "../../core/log.h"

#include <stddef.h>



// Structure for a global render state
typedef struct RenderState
{
    Matrix4 view_matrix;
    Matrix4 projection_matrix;
    Vector3 camera_pos;
    DirectionalLight global_light;
    PointLightData point_lights[MAX_RESOURCES];
    uint32_t point_light_count;
} RenderState;


// Global render state to be used by renderer
static RenderState state;



// This internal struct holds the actual OpenGL data
typedef struct GLMesh
{
    GLuint vao;
    GLuint vbo;
    GLuint ebo;
    uint32_t index_count;
    bool active;
} GLMesh;


typedef struct GLShader
{
    GLuint program; bool active;
} GLShader;


typedef struct GLTexture
{
    GLuint id; bool active;
} GLTexture;



// We use parallel arrays or flat arrays to store resources.
// MeshHandle.id will correspond to an index in this array.
static GLMesh mesh_pool[MAX_RESOURCES];
static GLShader shader_pool[MAX_RESOURCES];
static GLTexture texture_pool[MAX_RESOURCES];



// --- THE COMMAND QUEUE ---
typedef struct RenderCommand
{
    MeshHandle mesh;
    ShaderHandle shader;
    TextureHandle texture;
    MaterialProperties mat_props;
    Matrix4 transform;
} RenderCommand;



static RenderCommand command_queue[MAX_COMMANDS];
static uint32_t command_count = 0;

// Old Global frame state
// static Matrix4 current_view;
// static Matrix4 current_proj;
// static Vector3 current_camera_pos;
// static DirectionalLight current_light;

// static PointLightData current_point_lights[MAX_RESOURCES];
// static uint32_t current_point_light_count;




// Initialization function
bool Render_Init(GraphicsAPI api, Render_LoadProcFn load_proc)
{
    // Initialize our pools
    for (int i = 0; i < MAX_RESOURCES; i++)
    {
        mesh_pool[i].active = false;
        shader_pool[i].active = false;
        texture_pool[i].active = false;
    }


    // Select correct graphics backend
    if (api == GRAPHICS_API_OPENGL)
    {
        // 1. Load OpenGL functions using the provided loader
        if (!gladLoadGLLoader((GLADloadproc)load_proc)) {
            // LOG_ERROR("Failed to initialize OpenGL loader!");
            Log_Error("Failed to initialize OpenGL loader!");
            return false;
        }

        // 2. Set global OpenGL state
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_CULL_FACE); // Disables drawing the inside of a mesh
        // glDisable(GL_CULL_FACE);
        
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


// Shutdown function
void Render_Shutdown(void)
{
    // 1. Clear out any pending draw commands so nothing tries to render during shutdown
    command_count = 0;

    // 2. The Garbage Collector Loop
    // We start at 1 because index 0 is the "Invalid/Null" handle.
    for (uint32_t i = 1; i < MAX_RESOURCES; i++) {
        
        if (mesh_pool[i].active) {
            // Optional: You could add a LOG_WARN here to tell the user they 
            // had a memory leak during runtime!
            Render_DestroyMesh((MeshHandle){i});
        }
        
        if (texture_pool[i].active) {
            Render_DestroyTexture((TextureHandle){i});
        }
        
        if (shader_pool[i].active) {
            Render_DestroyShader((ShaderHandle){i});
        }
    }
}










// Functions to set viewport, colors, and clearing

void Render_SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    glViewport(x, y, width, height);
}

void Render_SetClearColor(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
}

void Render_Clear(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}















// --- Resource Creation API ---

MeshHandle Render_CreateMesh(const Vertex3D* vertices, uint32_t vertex_count, 
                             const uint32_t* indices,  uint32_t index_count)
{
    // Find an empty slot (in a real engine, use a free-list for $O(1)$ allocation)
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!mesh_pool[i].active)
        {
            id = i;
            break;
        }
    }

    if (id == 0) return (MeshHandle){0}; // Pool is full!

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

    // Define Vertex Attributes (Tying it to your Vertex3D struct layout)
    // 1. Position (Vector3)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, position));
    glEnableVertexAttribArray(0);
    // 2. Normal (Vector3)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, normal));
    glEnableVertexAttribArray(1);
    // 3. UV (Vector2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)offsetof(Vertex3D, uv));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0); // Unbind to prevent accidental modifications

    return (MeshHandle){id};
}





// TextureHandle Render_CreateTexture(const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels, TextureFilter filter)
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
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

    // Ship the pixels to VRAM!
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
            
            // Return the POOL INDEX, not the raw OpenGL ID!
            return (TextureHandle){ i }; 
        }
    }

    // Fall back if no more texture slots
    glDeleteTextures(1, &texture_id);

    return (TextureHandle){ 0 };
}





ShaderHandle Render_CreateShader(const char* vertex_source, const char* fragment_source)
{
    uint32_t id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!shader_pool[i].active)
        {
            id = i;
            break;
        }
    }

    if (id == 0) return (ShaderHandle){0};

    // Simplified compilation for brevity. In reality, check glGetShaderiv for GL_COMPILE_STATUS
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_source, NULL);
    glCompileShader(vs);

    int success;
    char infoLog[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vs, 512, NULL, infoLog);
        printf("ERROR: Vertex Shader Compilation Failed!\n%s\n", infoLog);
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_source, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fs, 512, NULL, infoLog);
        printf("ERROR: Fragment Shader Compilation Failed!\n%s\n", infoLog);
    }

    GLShader* shader = &shader_pool[id];
    shader->active = true;
    shader->program = glCreateProgram();
    glAttachShader(shader->program, vs);
    glAttachShader(shader->program, fs);
    glLinkProgram(shader->program);

    glGetProgramiv(shader->program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shader->program, 512, NULL, infoLog);
        printf("ERROR: Shader Program Linking Failed!\n%s\n", infoLog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return (ShaderHandle){id};
}















void Render_DestroyMesh(MeshHandle mesh)
{
    // 1. Validate the handle to prevent segfaults
    if (mesh.id == 0 || mesh.id >= MAX_RESOURCES)
        return;

    GLMesh* gl_mesh = &mesh_pool[mesh.id];
    
    // 2. Only delete if the slot is actually in use
    if (gl_mesh->active)
    {
        // Tell OpenGL to free the GPU memory
        glDeleteVertexArrays(1, &gl_mesh->vao);
        glDeleteBuffers(1, &gl_mesh->vbo);
        glDeleteBuffers(1, &gl_mesh->ebo);
        
        // 3. Mark the slot as free so Render_CreateMesh can reuse this ID later
        gl_mesh->active = false;
    }
}


void Render_DestroyTexture(TextureHandle texture)
{
    if (texture.id == 0 || texture.id >= MAX_RESOURCES)
        return;

    GLTexture* tex = &texture_pool[texture.id];
    if (tex->active)
    {
        glDeleteTextures(1, &tex->id);
        tex->active = false;
    }
}


void Render_DestroyShader(ShaderHandle shader)
{
    if (shader.id == 0 || shader.id >= MAX_RESOURCES)
        return;

    GLShader* gl_shader = &shader_pool[shader.id];
    if (gl_shader->active)
    {
        glDeleteProgram(gl_shader->program);
        gl_shader->active = false;
    }
}















void Render_BeginFrame(const RenderPacket* packet)
{
    state.view_matrix = packet->view_matrix;
    state.projection_matrix = packet->projection_matrix;
    state.camera_pos = packet->camera_pos;
    state.global_light = packet->global_light;

    state.point_light_count = packet->point_light_count;
    for (uint32_t i = 0; i < packet->point_light_count; i++)
        state.point_lights[i] = packet->point_lights[i];
    
    command_count = 0; // Reset the queue for the new frame
}



// void Render_Submit(MeshHandle mesh, ShaderHandle shader, TextureHandle texture, Matrix4 transform)
// void Render_Submit(MeshHandle mesh, MaterialHandle material, Matrix4 transform)
void Render_Submit(MeshHandle mesh, ShaderHandle shader, TextureHandle texture, MaterialProperties mat_props, Matrix4 transform)
{
    if (command_count >= MAX_COMMANDS) return; // Queue full
    
    command_queue[command_count++] = (RenderCommand){
        mesh,
        shader,
        texture,
        mat_props,
        transform
    };
}



void Render_EndFrame(void) {
    // In a AAA engine, you would sort the command_queue array here by ShaderHandle 
    // to minimize OpenGL state changes. For now, we just iterate.

    for (uint32_t i = 0; i < command_count; i++) {
        RenderCommand* cmd = &command_queue[i];

        if (!mesh_pool[cmd->mesh.id].active || !shader_pool[cmd->shader.id].active) continue;

        GLMesh* gl_mesh = &mesh_pool[cmd->mesh.id];
        GLShader* gl_shader = &shader_pool[cmd->shader.id];

        // 1. Bind Shader
        glUseProgram(gl_shader->program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture_pool[cmd->texture.id].id);

        // 3. Upload Matrices
        // Note: Caching these locations at shader creation time is much faster!
        GLint model_loc = glGetUniformLocation(gl_shader->program, "u_Model");
        GLint view_loc  = glGetUniformLocation(gl_shader->program, "u_View");
        GLint proj_loc  = glGetUniformLocation(gl_shader->program, "u_Projection");
        
        glUniformMatrix4fv(model_loc, 1, GL_FALSE, (float*)&cmd->transform);
        glUniformMatrix4fv(view_loc, 1, GL_FALSE, (float*)&state.view_matrix);
        glUniformMatrix4fv(proj_loc, 1, GL_FALSE, (float*)&state.projection_matrix);

        // --- NEW: UPLOAD LIGHTING DATA ---
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
        for (uint32_t j = 0; j < state.point_light_count; j++) {
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
        

        // 4. Draw
        glBindVertexArray(gl_mesh->vao);
        glDrawElements(GL_TRIANGLES, gl_mesh->index_count, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
}