#include "opengl_internal.h"





// Uploads vertex and index data to the GPU and returns a handle
MeshHandle OpenGL_CreateMesh(Renderer* r, const Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count)
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

    // --- Define Vertex Attributes ---

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





// Removes a mesh from the GPU
void OpenGL_DestroyMesh(Renderer* r, MeshHandle mesh)
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










// Uploads pixels to the renderer to make a texture. Returns a handle
TextureHandle OpenGL_CreateTexture(Renderer* r, const uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels)
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





// Uploads float HDR pixels to a texture
TextureHandle OpenGL_CreateTextureHDR(Renderer* r, const float* pixels, uint32_t width, uint32_t height, uint32_t channels)
{
    if (!pixels || width == 0 || height == 0 || channels == 0)
        return (TextureHandle){0};

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = GL_RGB;
    GLenum internal_format = GL_RGB16F;
    if (channels == 4)
    {
        format = GL_RGBA;
        internal_format = GL_RGBA16F;
    }

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_FLOAT, pixels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->texture_pool[i].active)
        {    
            internal->texture_pool[i].id = texture_id;
            internal->texture_pool[i].active = true;
            return (TextureHandle){ i }; 
        }
    }

    glDeleteTextures(1, &texture_id);

    return (TextureHandle){ 0 };
}





// Removes a texture from the GPU
void OpenGL_DestroyTexture(Renderer* r, TextureHandle texture)
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










// Converts an equirectangular HDR to Cubemap and generates Irradiance, Prefilter, and BRDF LUT maps
EnvironmentMap OpenGL_CreateEnvironmentMap(Renderer* r, const float* hdr_pixels, uint32_t width, uint32_t height)
{
    EnvironmentMap env = {0};
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    
    if (!hdr_pixels || width == 0 || height == 0)
        return env;

    TextureHandle hdr_tex = OpenGL_CreateTextureHDR(r, hdr_pixels, width, height, 3);
    if (hdr_tex.id == 0)
        return env;
    
    // We need 4 handles from the pool for skybox, irradiance, prefilter, brdf
    uint32_t ids[4] = {0,0,0,0};
    int found = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES && found < 4; i++)
    {
        if (!internal->texture_pool[i].active)
        {
            ids[found++] = i;
            internal->texture_pool[i].active = true; // Mark to reserve
        }
    }

    if (found < 4)
    {
        OpenGL_DestroyTexture(r, hdr_tex);

        for (int i = 0; i < found; i++)
            internal->texture_pool[ids[i]].active = false;
        
        return env;
    }
    
    env.skybox = (TextureHandle){ids[0]};
    env.irradiance = (TextureHandle){ids[1]};
    env.prefilter = (TextureHandle){ids[2]};
    env.brdf_lut = (TextureHandle){ids[3]};

    // Matrices for the 6 cube faces
    Matrix4 captureProjection = Matrix4Perspective(90.0f * (3.14159265359f / 180.0f), 1.0f, 0.1f, 10.0f);
    Matrix4 captureViews[] = {
       Matrix4LookAt((Vector3){0,0,0}, (Vector3){ 1, 0, 0}, (Vector3){0,-1, 0}),
       Matrix4LookAt((Vector3){0,0,0}, (Vector3){-1, 0, 0}, (Vector3){0,-1, 0}),
       Matrix4LookAt((Vector3){0,0,0}, (Vector3){ 0, 1, 0}, (Vector3){0, 0, 1}),
       Matrix4LookAt((Vector3){0,0,0}, (Vector3){ 0,-1, 0}, (Vector3){0, 0,-1}),
       Matrix4LookAt((Vector3){0,0,0}, (Vector3){ 0, 0, 1}, (Vector3){0,-1, 0}),
       Matrix4LookAt((Vector3){0,0,0}, (Vector3){ 0, 0,-1}, (Vector3){0,-1, 0})
    };
    
    GLuint envCubemap, irradianceMap, prefilterMap, brdfLUTTexture;
    


    // 1. Generate Env Cubemap
    glGenTextures(1, &envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    for (unsigned int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 512, 512, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Convert HDR to Cubemap
    GLuint prog = internal->shader_pool[internal->ibl.equirectangular_to_cubemap.id].program;
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "equirectangularMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, internal->texture_pool[hdr_tex.id].id);
    glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, (float*)&captureProjection);
    
    glViewport(0, 0, 512, 512);
    glBindFramebuffer(GL_FRAMEBUFFER, internal->ibl.capture_fbo);
    // The same capture renderbuffer is reused by local-probe prefilter mips, whose final pass leaves it at 8x8.
    // Restore matching 512x512 depth storage before attaching HDR cubemap faces or the framebuffer is incomplete.
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    for (unsigned int i = 0; i < 6; i++)
    {
        glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, (float*)&captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, envCubemap, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Log_Error("ERROR: HDR cubemap capture framebuffer is incomplete on face %u", i);
            break;
        }
        
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glBindVertexArray(internal->skybox.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);



    // 2. Generate Irradiance Map
    glGenTextures(1, &irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    for (unsigned int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ibl.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    
    prog = internal->shader_pool[internal->ibl.irradiance_convolution.id].program;
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "environmentMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, (float*)&captureProjection);

    glViewport(0, 0, 32, 32);
    for (unsigned int i = 0; i < 6; i++)
    {
        glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, (float*)&captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glBindVertexArray(internal->skybox.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
    }



    // 3. Generate Prefilter Map
    glGenTextures(1, &prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    for (unsigned int i = 0; i < 6; i++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    prog = internal->shader_pool[internal->ibl.prefilter.id].program;
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "environmentMap"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, (float*)&captureProjection);

    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; mip++)
    {
        unsigned int mipWidth  = 128 >> mip;
        unsigned int mipHeight = 128 >> mip;
        glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        glUniform1f(glGetUniformLocation(prog, "roughness"), roughness);
        for (unsigned int i = 0; i < 6; i++)
        {
            glUniformMatrix4fv(glGetUniformLocation(prog, "view"), 1, GL_FALSE, (float*)&captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilterMap, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glBindVertexArray(internal->skybox.vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
        }
    }



    // 4. Generate BRDF LUT
    glGenTextures(1, &brdfLUTTexture);
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ibl.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture, 0);
    
    glViewport(0, 0, 512, 512);
    prog = internal->shader_pool[internal->ibl.brdf.id].program;
    glUseProgram(prog);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindVertexArray(internal->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    // Cleanup temporary HDR texture as it's no longer needed, everything is in the cubemaps
    OpenGL_DestroyTexture(r, hdr_tex);
    
    internal->texture_pool[env.skybox.id].id = envCubemap;
    internal->texture_pool[env.irradiance.id].id = irradianceMap;
    internal->texture_pool[env.prefilter.id].id = prefilterMap;
    internal->texture_pool[env.brdf_lut.id].id = brdfLUTTexture;
    
    // Set viewport back
    OpenGL_SetViewport(r, 0, 0, internal->state.window_width, internal->state.window_height);

    return env;
}











// Uploads vertex and fragment shaders to the GPU to make a complete shader. Returns a handle
ShaderHandle OpenGL_CreateShader(Renderer* r, const char* vertex_source, const char* fragment_source)
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
        Log_Error("ERROR: Vertex Shader Compilation Failed.\nInfo: %s\n", infoLog);
    }

    // Create fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_source, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fs, 512, NULL, infoLog);
        Log_Error("ERROR: Fragment Shader Compilation Failed.\nInfo: %s\n", infoLog);
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
        Log_Error("ERROR: Shader Program Linking Failed.\nInfo: %s\n", infoLog);
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return (ShaderHandle){id};
}





// Helper function to compile an internal engine shader from raw source code strings
ShaderHandle OpenGL_CompileInternalShader(OpenGL_Backend* internal, const char* name, const char* vertex_src, const char* geom_src, const char* fragment_src)
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
        Log_Error("ERROR: Failed to allocate internal shader: %s (Pool full)", name);
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
        Log_Error("ERROR: Internal Vertex Shader Compilation Failed (%s):\n%s", name, infoLog);
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
            Log_Error("ERROR: Internal Geometry Shader Compilation Failed (%s):\n%s", name, infoLog);
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
        Log_Error("ERROR: Internal Fragment Shader Compilation Failed (%s):\n%s", name, infoLog);
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
        Log_Error("ERROR: Internal Shader Linking Failed (%s):\n%s", name, infoLog);
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
ShaderHandle OpenGL_CompileInternalShaderFromFile(OpenGL_Backend* internal, const char* name, const char* vert_path, const char* geom_path, const char* frag_path)
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





// Removes a shader from the GPU
void OpenGL_DestroyShader(Renderer* r, ShaderHandle shader)
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










// Creates a CubeMap texture. Returns a texture handle
TextureHandle OpenGL_CreateCubemap(Renderer* r, const uint8_t* right, const uint8_t* left, const uint8_t* top, const uint8_t* bottom, const uint8_t* front, const uint8_t* back, uint32_t width, uint32_t height, uint32_t channels)
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
MeshHandle OpenGL_CreateSkinnedMesh(Renderer* r, const Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices,  uint32_t index_count)
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
MeshHandle OpenGL_CreateDynamicMesh(Renderer* r, uint32_t max_vertices, uint32_t max_indices)
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

    // --- Define Vertex Attributes ---

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
void OpenGL_UpdateDynamicMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
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










// Overwrites the GPU memory with new vertex data
void OpenGL_UpdateMesh(Renderer* r, MeshHandle handle, Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count)
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
    mesh->max_vertices = vertex_count;
    mesh->max_indices = index_count;

    // Overwrite the buffers
    glBindVertexArray(mesh->vao);

    // Vertex Buffer
    glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex3D), vertices, GL_STATIC_DRAW);

    // Element Buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(uint32_t), indices, GL_STATIC_DRAW);

    glBindVertexArray(0);
}










// Rotates an images pixels by 90 degrees CW
uint8_t* OpenGL_RotatePixels90CW(const uint8_t* src, int w, int h, int c)
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
uint8_t* OpenGL_RotatePixels90CCW(const uint8_t* src, int w, int h, int c)
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