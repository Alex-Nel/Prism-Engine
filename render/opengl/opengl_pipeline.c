#include "opengl_internal.h"





// Binds the SSAO result (or a white fallback when SSAO is disabled).
void OpenGL_BindSSAOTexture(OpenGL_Backend* internal, GLuint program)
{
    GLint ssao_loc = glGetUniformLocation(program, "ssaoMap");
    if (ssao_loc == -1)
        return;

    glUniform1i(ssao_loc, 5);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, internal->state.settings.enable_ssao ? internal->ssao.ssaoColorBufferBlur : internal->ssao.fallbackWhiteTexture);
}










// Copies shadow-map state from a render packet into the backend.
void OpenGL_CopyShadowState(RenderState* state, const RenderPacket* packet)
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
void OpenGL_UploadShadowUniforms(GLuint program, const RenderState* state)
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
void OpenGL_DrawShadowQueue(OpenGL_Backend* internal, const Matrix4* light_space_matrix)
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
void OpenGL_BeginShadowPass(Renderer* r, const RenderPacket* packet)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    OpenGL_CopyShadowState(&internal->state, packet);

    // Reset the command queue for the shadow pass
    internal->command_count = 0;
}










// Ends the shadow pass and resets the state
void OpenGL_EndShadowPass(Renderer* r)
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















// --- OpenGL Render Pipeline Functions ---

// Restores the default window framebuffer for forward rendering.
void OpenGL_BindDefaultFramebuffer()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glReadBuffer(GL_BACK);
}










// Uploads generic renderer uniforms
void OpenGL_UploadCommonUniforms(GLuint program, const RenderState* state)
{
    GLint gamma_loc = glGetUniformLocation(program, "u_Gamma");
    if (gamma_loc != -1)
        glUniform1f(gamma_loc, state->settings.gamma > 0.01f ? state->settings.gamma : 2.2f);

    GLint ambient_color_loc = glGetUniformLocation(program, "u_GlobalAmbientColor");
    if (ambient_color_loc != -1)
        glUniform3fv(ambient_color_loc, 1, (float*)&state->global_ambient_color);

    GLint ambient_int_loc = glGetUniformLocation(program, "u_GlobalAmbientIllumination");
    if (ambient_int_loc != -1)
        glUniform1f(ambient_int_loc, state->global_ambient_illumination);
}










// Extracts the string formatting for lights
void OpenGL_UploadLightUniforms(GLuint program, const RenderState* state)
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
void OpenGL_UploadDirectionalLightUniforms(GLuint program, const RenderState* state)
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
void ExecuteGBufferPass(OpenGL_Backend* internal, uint32_t opaque_count)
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
void ExecuteDeferredLightingPass(OpenGL_Backend* internal)
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
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, internal->state.settings.enable_ssao ? internal->ssao.ssaoColorBufferBlur : internal->ssao.fallbackWhiteTexture); glUniform1i(glGetUniformLocation(def_prog, "ssaoMap"), 3);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray); glUniform1i(glGetUniformLocation(def_prog, "shadowMap"), 4);
    
    // Upload Uniforms
    glUniformMatrix4fv(glGetUniformLocation(def_prog, "u_View"), 1, GL_FALSE, (float*)&internal->state.view_matrix);
    glUniform3fv(glGetUniformLocation(def_prog, "u_ViewPos"), 1, (float*)&internal->state.camera_pos);
    glUniform1i(glGetUniformLocation(def_prog, "u_EnableSSAO"), internal->state.settings.enable_ssao ? 1 : 0);
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
void ExecuteSSAOPass(OpenGL_Backend* internal)
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
void OpenGL_RenderCommandBatch(OpenGL_Backend* internal, uint32_t start_idx, uint32_t end_idx)
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
                glUniform1i(enable_ssao_loc, internal->state.settings.enable_ssao ? 1 : 0);

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
void OpenGL_DrawSkybox(OpenGL_Backend* internal)
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










// Sets the global camera matrices for the current frame
void OpenGL_BeginFrame(Renderer* r, const RenderPacket* packet)
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
    internal->state.settings.enable_ssao = packet->enable_ssao;
    internal->state.global_ambient_color = packet->global_ambient_color;
    internal->state.global_ambient_illumination = packet->global_ambient_illumination;
    if (packet->gamma > 0.01f)
        internal->state.settings.gamma = packet->gamma;
    else
        internal->state.settings.gamma = internal->state.settings.gamma > 0.01f ? internal->state.settings.gamma : 2.2f;
    
    if (packet->skybox_shader.id != 0 && packet->skybox_shader.id < MAX_RESOURCES && internal->shader_pool[packet->skybox_shader.id].active)
        internal->skybox.default_shader = packet->skybox_shader;

    // Reset the queue for the new frame
    internal->command_count = 0;
}










// Adds an object to the draw queue
void OpenGL_Submit(Renderer* r, MeshHandle mesh, ShaderHandle shader,
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










// Sorts the queue, binds the state, and executes the actual GPU draw calls
void OpenGL_EndFrame(Renderer* r)
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
    if (internal->state.settings.enable_ssao && internal->ssao.g_buffer_shader.id != 0 &&
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

static int CompareRenderCommands(const void* a, const void* b);