#include "opengl_internal.h"
#include "../shadow_cascades.h"





// Extracts the view frustum from a render state
static Frustum OpenGL_ExtractViewFrustum(const RenderState* state)
{
    Matrix4 view_proj = Matrix4Multiply(state->projection_matrix, state->view_matrix);
    return Frustum_ExtractFromMatrix(view_proj);
}





// Returns whether an item is in a specific frustum
static bool OpenGL_ItemInFrustum(const RenderItem* item, Frustum* frustum)
{
    return Frustum_ContainsAABB(frustum, item->local_bounds, item->transform);
}





// Returns the GLMaterial based on a handle
static const GLMaterial* OpenGL_GetMaterial(OpenGL_Backend* internal, MaterialHandle handle)
{
    if (handle.id == 0 || handle.id >= MAX_RESOURCES)
        return NULL;

    GLMaterial* mat = &internal->material_pool[handle.id];
    return mat->active ? mat : NULL;
}





// Returns a shader handle for a specific material
static ShaderHandle OpenGL_MaterialShader(OpenGL_Backend* internal, MaterialHandle handle)
{
    const GLMaterial* mat = OpenGL_GetMaterial(internal, handle);
    if (mat)
        return mat->shader;

    return (ShaderHandle){0};
}





// Returns whether a texture handle is valid
static bool OpenGL_TextureValid(OpenGL_Backend* internal, TextureHandle handle)
{
    return handle.id != 0 && handle.id < MAX_RESOURCES && internal->texture_pool[handle.id].active && internal->texture_pool[handle.id].id != 0;
}





// Returns the ID of an OpenGL environment map
static uint32_t OpenGL_EnvSkyboxTextureId(OpenGL_Backend* internal, EnvironmentMapHandle handle)
{
    GLEnvironmentMap* env = OpenGL_GetEnvMap(internal, handle);
    return env ? env->skybox.id : 0;
}






// Applies a material
static void OpenGL_ApplyMaterial(OpenGL_Backend* internal, GLuint program, MaterialHandle handle, Color instance_color, bool gbuffer_tint)
{
    const GLMaterial* mat = OpenGL_GetMaterial(internal, handle);
    TextureHandle albedo = {0};
    TextureHandle normal = {0};
    TextureHandle metallic = {0};
    TextureHandle roughness = {0};
    TextureHandle ao = {0};
    float metallic_factor = 0.0f;
    float roughness_factor = 0.5f;

    if (mat)
    {
        albedo = mat->albedo;
        normal = mat->normal;
        metallic = mat->metallic;
        roughness = mat->roughness;
        ao = mat->ao;
        metallic_factor = mat->properties.metallic_factor;
        roughness_factor = mat->properties.roughness_factor;
    }

    glActiveTexture(GL_TEXTURE0);
    bool valid_albedo = OpenGL_TextureValid(internal, albedo);
    glBindTexture(GL_TEXTURE_2D, valid_albedo ? internal->texture_pool[albedo.id].id : internal->texture_pool[1].id);
    glUniform1i(glGetUniformLocation(program, "u_Material.albedoMap"), 0);
    if (!gbuffer_tint)
        glUniform1i(glGetUniformLocation(program, "u_Material.diffuse"), 0);
    
    bool valid_normal = OpenGL_TextureValid(internal, normal);
    glUniform1i(glGetUniformLocation(program, "u_Material.hasNormalMap"), valid_normal ? 1 : 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, valid_normal ? internal->texture_pool[normal.id].id : internal->texture_pool[2].id);
    glUniform1i(glGetUniformLocation(program, "u_Material.normalMap"), 1);
    
    bool valid_metallic = OpenGL_TextureValid(internal, metallic);
    glUniform1i(glGetUniformLocation(program, "u_Material.hasMetallicMap"), valid_metallic ? 1 : 0);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, valid_metallic ? internal->texture_pool[metallic.id].id : internal->texture_pool[3].id);
    glUniform1i(glGetUniformLocation(program, "u_Material.metallicMap"), 2);
    
    bool valid_roughness = OpenGL_TextureValid(internal, roughness);
    glUniform1i(glGetUniformLocation(program, "u_Material.hasRoughnessMap"), valid_roughness ? 1 : 0);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, valid_roughness ? internal->texture_pool[roughness.id].id : internal->texture_pool[1].id);
    glUniform1i(glGetUniformLocation(program, "u_Material.roughnessMap"), 3);
    
    bool valid_ao = OpenGL_TextureValid(internal, ao);
    glUniform1i(glGetUniformLocation(program, "u_Material.hasAOMap"), valid_ao ? 1 : 0);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, valid_ao ? internal->texture_pool[ao.id].id : internal->texture_pool[1].id);
    glUniform1i(glGetUniformLocation(program, "u_Material.aoMap"), 4);
    
    const char* tint_name = gbuffer_tint ? "u_Material.albedoTint" : "u_Material.tint";
    glUniform3fv(glGetUniformLocation(program, tint_name), 1, (float*)&instance_color);
    glUniform1f(glGetUniformLocation(program, "u_Material.metallicFactor"), metallic_factor);
    glUniform1f(glGetUniformLocation(program, "u_Material.roughnessFactor"), roughness_factor);
}










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










// Applies cascade matrices computed by the shared helper into backend render state.
static void OpenGL_ApplyCascadeResult(RenderState* state, const ShadowCascadeResult* result)
{
    state->shadow_cascade_count = result->cascade_count;
   
    if (state->shadow_cascade_count < 1)
        state->shadow_cascade_count = 1;
    
    if (state->shadow_cascade_count > MAX_SHADOW_CASCADES)
        state->shadow_cascade_count = MAX_SHADOW_CASCADES;

    for (uint32_t i = 0; i < state->shadow_cascade_count; i++)
    {
        state->light_space_matrices[i] = result->light_space_matrices[i];
        state->shadow_texel_world_sizes[i] = result->shadow_texel_world_sizes[i];
    }

    for (uint32_t i = 1; i < state->shadow_cascade_count; i++)
        state->cascade_splits[i - 1] = result->cascade_splits[i - 1];

    state->cascade_blend_fraction = result->cascade_blend_fraction;
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
    Frustum light_frustum = Frustum_ExtractFromMatrix(*light_space_matrix);

    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        RenderItem* cmd = &internal->command_queue[i];

        if (cmd->mesh.id == 0 || cmd->mesh.id >= MAX_RESOURCES || !internal->mesh_pool[cmd->mesh.id].active)
            continue;

        if ((cmd->flags & RENDER_ITEM_CAST_SHADOWS) == 0)
            continue;

        if (!OpenGL_ItemInFrustum(cmd, &light_frustum))
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










// Fills directional, spot, and point shadow maps from the current item queue
void OpenGL_ExecuteShadowPass(OpenGL_Backend* internal)
{
    bool any_dir_shadows = false;
    if (internal->state.dir_light_count > 0 && internal->state.dir_lights[0].casts_shadows)
        any_dir_shadows = true;

    bool any_local_shadows = false;
    for (uint32_t i = 0; i < internal->state.spot_light_count && !any_local_shadows; i++)
    {
        if (internal->state.spot_lights[i].casts_shadows)
            any_local_shadows = true;
    }
    for (uint32_t i = 0; i < internal->state.point_light_count && !any_local_shadows; i++)
    {
        if (internal->state.point_lights[i].casts_shadows)
            any_local_shadows = true;
    }

    if (!any_dir_shadows && !any_local_shadows)
        return;
    if (any_dir_shadows)
    {
        ShadowCascadeCamera cascade_camera = {0};
        cascade_camera.position = internal->state.shadow_camera_pos;
        cascade_camera.forward = internal->state.camera_forward;
        cascade_camera.right = internal->state.camera_right;
        cascade_camera.up = internal->state.camera_up;
        cascade_camera.near_z = internal->state.shadow_camera_near;
        cascade_camera.far_z = internal->state.shadow_camera_far;
        cascade_camera.fov = internal->state.shadow_camera_fov;
        cascade_camera.aspect = internal->state.shadow_camera_aspect;
        ShadowCascadeResult cascade_result;
        Render_ComputeDirectionalCascades(&internal->state.dir_lights[0], &cascade_camera, internal->state.settings.shadow_map_resolution, &cascade_result);
        OpenGL_ApplyCascadeResult(&internal->state, &cascade_result);
    }

    glDisable(GL_CULL_FACE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0f, 1.0f);

    // --- Directional Light Cascades ---
    if (any_dir_shadows)
    {
        uint32_t cascade_count = internal->state.shadow_cascade_count;
        if (cascade_count < 1)
            cascade_count = 1;

        glBindFramebuffer(GL_FRAMEBUFFER, internal->shadow.depthMapFBO);
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
            RenderItem* cmd = &internal->command_queue[c];
            
            if (cmd->mesh.id == 0 || cmd->mesh.id >= MAX_RESOURCES || !internal->mesh_pool[cmd->mesh.id].active)
                continue;

            if ((cmd->flags & RENDER_ITEM_CAST_SHADOWS) == 0)
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

    glViewport(0, 0, internal->state.window_width, internal->state.window_height);
}















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

    GLint exp_loc = glGetUniformLocation(program, "u_Exposure");
    if (exp_loc != -1)
        glUniform1f(exp_loc, state->settings.exposure > 0.001f ? state->settings.exposure : 1.0f);

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
    Frustum camera_frustum = OpenGL_ExtractViewFrustum(&internal->state);

    for (uint32_t i = 0; i < opaque_count; i++)
    {
        RenderItem* cmd = &internal->command_queue[i];
        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        if (!OpenGL_ItemInFrustum(cmd, &camera_frustum))
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


        OpenGL_ApplyMaterial(internal, g_prog->program, cmd->material, cmd->color, true);

        glUniform1f(glGetUniformLocation(g_prog->program, "u_ReceiveShadows"), (cmd->flags & RENDER_ITEM_RECEIVE_SHADOWS) ? 1.0f : 0.0f);

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

    GLEnvironmentMap* global_env = OpenGL_GetEnvMap(internal, internal->state.env_map);
    bool has_global_ibl = internal->state.has_env_map && global_env && global_env->has_ibl;

    // Bind IBL Maps
    glUniform1i(glGetUniformLocation(def_prog, "u_HasIBL"), has_global_ibl ? 1 : 0);
    const int ibl_debug_mode = 0; // Set to 1 for compressed RGB irradiance or 2 for logarithmic luminance.
    glUniform1i(glGetUniformLocation(def_prog, "u_IBLDebugMode"), ibl_debug_mode);
    if (has_global_ibl)
    {
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGL_TextureGL(internal, global_env->irradiance)); glUniform1i(glGetUniformLocation(def_prog, "irradianceMap"), 5);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGL_TextureGL(internal, global_env->prefilter)); glUniform1i(glGetUniformLocation(def_prog, "prefilterMap"), 6);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, OpenGL_TextureGL(internal, global_env->brdf_lut)); glUniform1i(glGetUniformLocation(def_prog, "brdfLUT"), 7);
    }
    
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


    // --- Local IBL probe replacement volumes ---
    if (internal->state.reflection_probe_count > 0 && internal->deferred.probe_volume_shader.id != 0)
    {
        GLuint probe_program = internal->shader_pool[internal->deferred.probe_volume_shader.id].program;
        glUseProgram(probe_program);

        glUniform1i(glGetUniformLocation(probe_program, "gPosition"), 0);
        glUniform1i(glGetUniformLocation(probe_program, "gNormal"), 1);
        glUniform1i(glGetUniformLocation(probe_program, "gAlbedoSpec"), 2);
        glUniform1i(glGetUniformLocation(probe_program, "ssaoMap"), 3);
        glUniform1i(glGetUniformLocation(probe_program, "localIrradianceMap"), 4);
        glUniform1i(glGetUniformLocation(probe_program, "localPrefilterMap"), 5);
        glUniform1i(glGetUniformLocation(probe_program, "globalIrradianceMap"), 6);
        glUniform1i(glGetUniformLocation(probe_program, "globalPrefilterMap"), 7);
        glUniform1i(glGetUniformLocation(probe_program, "brdfLUT"), 8);
        glUniform1i(glGetUniformLocation(probe_program, "u_IBLDebugMode"), ibl_debug_mode);

        glUniform3fv(glGetUniformLocation(probe_program, "u_ViewPos"), 1, (float*)&internal->state.camera_pos);
        glUniform2f(glGetUniformLocation(probe_program, "u_ScreenSize"), (float)internal->state.window_width, (float)internal->state.window_height);
        glUniform1i(glGetUniformLocation(probe_program, "u_EnableSSAO"), internal->state.settings.enable_ssao ? 1 : 0);
        OpenGL_UploadCommonUniforms(probe_program, &internal->state);

        bool has_global_ibl = internal->state.has_env_map && global_env && global_env->has_ibl;
        glUniform1i(glGetUniformLocation(probe_program, "u_HasGlobalIBL"), has_global_ibl ? 1 : 0);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, has_global_ibl ? OpenGL_TextureGL(internal, global_env->irradiance) : 0);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, has_global_ibl ? OpenGL_TextureGL(internal, global_env->prefilter) : 0);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, has_global_ibl ? OpenGL_TextureGL(internal, global_env->brdf_lut) : 0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(internal->quad_vao);

        for (uint32_t probe_index = 0; probe_index < internal->state.reflection_probe_count; probe_index++)
        {
            ReflectionProbeData* probe = &internal->state.reflection_probes[probe_index];
            GLReflectionProbe* captured = NULL;
            for (uint32_t slot_index = 0; slot_index < MAX_REFLECTION_PROBES; slot_index++)
            {
                GLReflectionProbe* candidate = &internal->reflection_probes[slot_index];
                if (candidate->active && candidate->entity_id == probe->entity_id)
                {
                    captured = candidate;
                    break;
                }
            }

            GLEnvironmentMap* captured_env = captured ? OpenGL_GetEnvMap(internal, captured->environment) : NULL;
            if (!captured_env || captured_env->irradiance.id == 0)
                continue;

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGL_TextureGL(internal, captured_env->irradiance));
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_CUBE_MAP, OpenGL_TextureGL(internal, captured_env->prefilter));

            if (!has_global_ibl && captured_env->brdf_lut.id != 0)
            {
                glActiveTexture(GL_TEXTURE8);
                 glBindTexture(GL_TEXTURE_2D, OpenGL_TextureGL(internal, captured_env->brdf_lut));
            }

            Vector3 box_min = Vector3Subtract(probe->position, probe->box_extents);
            Vector3 box_max = Vector3Add(probe->position, probe->box_extents);

            glUniform3fv(glGetUniformLocation(probe_program, "u_ProbePosition"), 1, (float*)&probe->position);
            glUniform3fv(glGetUniformLocation(probe_program, "u_ProbeBoxMin"), 1, (float*)&box_min);
            glUniform3fv(glGetUniformLocation(probe_program, "u_ProbeBoxMax"), 1, (float*)&box_max);
            glUniform1f(glGetUniformLocation(probe_program, "u_ProbeBlendDistance"), probe->blend_distance);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_BLEND);
    }



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
    for (uint32_t i = 0; i < internal->state.point_light_count && ibl_debug_mode == 0; i++)
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
    for (uint32_t i = 0; i < internal->state.spot_light_count && ibl_debug_mode == 0; i++)
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










// Executes a forward rendering loop. Probe capture forces the engine's linear forward shaders so custom display shaders cannot tone-map captured radiance.
static void OpenGL_RenderCommandBatchMode(OpenGL_Backend* internal, uint32_t start_idx, uint32_t end_idx, bool probe_capture)
{
    uint32_t current_shader = 0;
    uint32_t current_texture = 0;
    Frustum view_frustum = OpenGL_ExtractViewFrustum(&internal->state);

    for (uint32_t i = start_idx; i < end_idx; i++)
    {
        RenderItem* cmd = &internal->command_queue[i];
        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        if (probe_capture && (cmd->flags & RENDER_ITEM_PROBE_CAPTURE) == 0)
            continue;

        if (!OpenGL_ItemInFrustum(cmd, &view_frustum))
            continue;

        ShaderHandle target_handle = probe_capture ? (ShaderHandle){0} : OpenGL_MaterialShader(internal, cmd->material);
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
                glUniform1i(enable_ssao_loc, probe_capture ? 0 : (internal->state.settings.enable_ssao ? 1 : 0));

            OpenGL_UploadLightUniforms(gl_shader->program, &internal->state);

            GLint capture_loc = glGetUniformLocation(gl_shader->program, "u_CaptureLinearRadiance");
            if (capture_loc != -1)
                glUniform1i(capture_loc, probe_capture ? 1 : 0);

            if (probe_capture)
            {
                GLint ambient_loc = glGetUniformLocation(gl_shader->program, "u_GlobalAmbientIllumination");
                if (ambient_loc != -1)
                    glUniform1f(ambient_loc, 0.0f);
            }
        }

        OpenGL_ApplyMaterial(internal, gl_shader->program, cmd->material, cmd->color, false);

        glUniformMatrix4fv(glGetUniformLocation(gl_shader->program, "u_Model"), 1, GL_FALSE, (float*)&cmd->transform);
        glUniform1f(glGetUniformLocation(gl_shader->program, "u_ReceiveShadows"), (!probe_capture && (cmd->flags & RENDER_ITEM_RECEIVE_SHADOWS)) ? 1.0f : 0.0f);

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










void OpenGL_RenderCommandBatch(OpenGL_Backend* internal, uint32_t start_idx, uint32_t end_idx)
{
    OpenGL_RenderCommandBatchMode(internal, start_idx, end_idx, false);
}










static void OpenGL_ReleaseProbeEnvironment(OpenGL_Backend* internal, EnvironmentMapHandle handle)
{
    OpenGL_DestroyEnvMapInternal(internal, handle);
}





static EnvironmentMapHandle OpenGL_ReserveProbeEnvironment(OpenGL_Backend* internal)
{
    EnvironmentMapHandle invalid = {0};
    uint32_t env_id = 0;
    for (uint32_t i = 1; i < MAX_RESOURCES; i++)
    {
        if (!internal->env_map_pool[i].active)
        {
            env_id = i;
            break;
        }
    }

    if (env_id == 0)
        return invalid;

    uint32_t ids[3] = {0, 0, 0};
    uint32_t found = 0;

    for (uint32_t i = 4; i < MAX_RESOURCES && found < 3; i++)
    {
        if (!internal->texture_pool[i].active)
        {
            ids[found++] = i;
            internal->texture_pool[i].active = true;
            internal->texture_pool[i].id = 0;
        }
    }

    if (found != 3)
    {
        for (uint32_t i = 0; i < found; i++)
            internal->texture_pool[ids[i]].active = false;

        return invalid;
    }

    GLEnvironmentMap* env = &internal->env_map_pool[env_id];
    memset(env, 0, sizeof(*env));
    env->active = true;
    env->skybox = (TextureHandle){ids[0]};
    env->irradiance = (TextureHandle){ids[1]};
    env->prefilter = (TextureHandle){ids[2]};
    GLEnvironmentMap* probe_src = OpenGL_GetEnvMap(internal, internal->state.probe_source_env_map);
    env->brdf_lut = probe_src ? probe_src->brdf_lut : (TextureHandle){0};
    env->has_ibl = true;
    env->owns_skybox = true;
    env->owns_irradiance = true;
    env->owns_prefilter = true;
    env->owns_brdf_lut = false;


    return (EnvironmentMapHandle){env_id};;
}










static void OpenGL_GetCubemapCaptureMatrices(Matrix4* projection, Matrix4 views[6], Vector3 position)
{
    *projection = Matrix4Perspective(90.0f * (3.14159265359f / 180.0f), 1.0f, 0.1f, 500.0f);
    views[0] = Matrix4LookAt(position, Vector3Add(position, (Vector3){ 1, 0, 0}), (Vector3){0,-1, 0});
    views[1] = Matrix4LookAt(position, Vector3Add(position, (Vector3){-1, 0, 0}), (Vector3){0,-1, 0});
    views[2] = Matrix4LookAt(position, Vector3Add(position, (Vector3){ 0, 1, 0}), (Vector3){0, 0, 1});
    views[3] = Matrix4LookAt(position, Vector3Add(position, (Vector3){ 0,-1, 0}), (Vector3){0, 0,-1});
    views[4] = Matrix4LookAt(position, Vector3Add(position, (Vector3){ 0, 0, 1}), (Vector3){0,-1, 0});
    views[5] = Matrix4LookAt(position, Vector3Add(position, (Vector3){ 0, 0,-1}), (Vector3){0,-1, 0});
}










static bool OpenGL_ConvolveProbeCubemap(OpenGL_Backend* internal, GLEnvironmentMap* environment, GLuint radiance_cubemap)
{
    Matrix4 capture_projection;
    Matrix4 capture_views[6];
    OpenGL_GetCubemapCaptureMatrices(&capture_projection, capture_views, (Vector3){0, 0, 0});

    GLuint irradiance_map = 0;
    glGenTextures(1, &irradiance_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradiance_map);

    for (uint32_t face = 0; face < 6; face++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, NULL);
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ibl.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    GLuint program = internal->shader_pool[internal->ibl.irradiance_convolution.id].program;
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "environmentMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, (float*)&capture_projection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, radiance_cubemap);
    glViewport(0, 0, 32, 32);

    for (uint32_t face = 0; face < 6; face++)
    {
        glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, (float*)&capture_views[face]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, irradiance_map, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindVertexArray(internal->skybox.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    GLuint prefilter_map = 0;
    glGenTextures(1, &prefilter_map);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map);
    
    for (uint32_t face = 0; face < 6; face++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, NULL);
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    program = internal->shader_pool[internal->ibl.prefilter.id].program;
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "environmentMap"), 0);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, (float*)&capture_projection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, radiance_cubemap);

    const uint32_t mip_count = 5;
    for (uint32_t mip = 0; mip < mip_count; mip++)
    {
        uint32_t mip_width = 128u >> mip;
        uint32_t mip_height = 128u >> mip;
        glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_width, mip_height);
        glViewport(0, 0, mip_width, mip_height);
        glUniform1f(glGetUniformLocation(program, "roughness"), (float)mip / (float)(mip_count - 1));

        for (uint32_t face = 0; face < 6; face++)
        {
            glUniformMatrix4fv(glGetUniformLocation(program, "view"), 1, GL_FALSE, (float*)&capture_views[face]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, prefilter_map, mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glBindVertexArray(internal->skybox.vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }

    internal->texture_pool[environment->irradiance.id].id = irradiance_map;
    internal->texture_pool[environment->prefilter.id].id = prefilter_map;

    return true;
}










static bool OpenGL_CaptureReflectionProbe(OpenGL_Backend* internal, const ReflectionProbeData* probe, uint32_t opaque_count, EnvironmentMapHandle* environment)
{
    if (internal->forward.default_shader.id == 0 || internal->ibl.irradiance_convolution.id == 0 || internal->ibl.prefilter.id == 0)
        return false;

    EnvironmentMapHandle handle = OpenGL_ReserveProbeEnvironment(internal);
    if (handle.id == 0)
        return false;

    GLEnvironmentMap* env = OpenGL_GetEnvMap(internal, handle);
    if (!env)
    {
        OpenGL_ReleaseProbeEnvironment(internal, handle);
        return false;
    }

    *environment = handle;

    uint32_t resolution = probe->capture_resolution;
    if (resolution < 32)  resolution = 32;
    if (resolution > 512) resolution = 512;

    GLuint radiance_cubemap = 0;
    glGenTextures(1, &radiance_cubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, radiance_cubemap);

    for (uint32_t face = 0; face < 6; face++)
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, NULL);
    
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    internal->texture_pool[env->skybox.id].id = radiance_cubemap;

    Matrix4 saved_view = internal->state.view_matrix;
    Matrix4 saved_projection = internal->state.projection_matrix;
    Vector3 saved_camera_position = internal->state.camera_pos;

    float saved_ambient_strengths[MAX_DIR_LIGHTS];
    for (uint32_t i = 0; i < internal->state.dir_light_count; i++)
    {
        saved_ambient_strengths[i] = internal->state.dir_lights[i].ambient_strength;
        internal->state.dir_lights[i].ambient_strength = 0.0f;
    }

    GLint old_viewport[4];
    GLfloat old_clear_color[4];
    glGetIntegerv(GL_VIEWPORT, old_viewport);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, old_clear_color);

    Matrix4 capture_projection;
    Matrix4 capture_views[6];
    OpenGL_GetCubemapCaptureMatrices(&capture_projection, capture_views, probe->position);
    internal->state.projection_matrix = capture_projection;
    internal->state.camera_pos = probe->position;

    glBindFramebuffer(GL_FRAMEBUFFER, internal->ibl.capture_fbo);
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, resolution, resolution);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    for (uint32_t face = 0; face < 6; face++)
    {
        internal->state.view_matrix = capture_views[face];
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, radiance_cubemap, 0);
        
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            OpenGL_ReleaseProbeEnvironment(internal, handle);
            *environment = (EnvironmentMapHandle){0};
            internal->state.view_matrix = saved_view;
            internal->state.projection_matrix = saved_projection;
            internal->state.camera_pos = saved_camera_position;
            
            for (uint32_t i = 0; i < internal->state.dir_light_count; i++)
                internal->state.dir_lights[i].ambient_strength = saved_ambient_strengths[i];
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
            glClearColor(old_clear_color[0], old_clear_color[1], old_clear_color[2], old_clear_color[3]);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
            glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);

            return false;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        OpenGL_RenderCommandBatchMode(internal, 0, opaque_count, true);

        GLEnvironmentMap* probe_src = OpenGL_GetEnvMap(internal, internal->state.probe_source_env_map);
        GLuint probe_src_skybox = probe_src ? OpenGL_TextureGL(internal, probe_src->skybox) : 0;

        if (internal->state.has_probe_source_env_map &&
            probe_src_skybox != 0 &&
            internal->ibl.probe_skybox.id > 0 &&
            internal->shader_pool[internal->ibl.probe_skybox.id].active)
        {
            GLuint sky_program = internal->shader_pool[internal->ibl.probe_skybox.id].program;
            glDepthFunc(GL_LEQUAL);
            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glUseProgram(sky_program);
            glUniformMatrix4fv(glGetUniformLocation(sky_program, "u_View"), 1, GL_FALSE, (float*)&capture_views[face]);
            glUniformMatrix4fv(glGetUniformLocation(sky_program, "u_Projection"), 1, GL_FALSE, (float*)&capture_projection);
            glUniform1i(glGetUniformLocation(sky_program, "u_IsHDR"), probe_src->has_ibl ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, probe_src_skybox);
            glUniform1i(glGetUniformLocation(sky_program, "u_Skybox"), 0);
            glBindVertexArray(internal->skybox.vao);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glEnable(GL_CULL_FACE);
        }
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, radiance_cubemap);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    bool success = OpenGL_ConvolveProbeCubemap(internal, env, radiance_cubemap);
    if (!success)
    {
        OpenGL_ReleaseProbeEnvironment(internal, handle);
        *environment = (EnvironmentMapHandle){0};
    }

    internal->state.view_matrix = saved_view;
    internal->state.projection_matrix = saved_projection;
    internal->state.camera_pos = saved_camera_position;
    for (uint32_t i = 0; i < internal->state.dir_light_count; i++)
        internal->state.dir_lights[i].ambient_strength = saved_ambient_strengths[i];

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(old_viewport[0], old_viewport[1], old_viewport[2], old_viewport[3]);
    glClearColor(old_clear_color[0], old_clear_color[1], old_clear_color[2], old_clear_color[3]);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Leave the shared IBL capture target in its canonical size so an HDR
    // environment loaded later can immediately render complete 512x512 faces.
    glBindRenderbuffer(GL_RENDERBUFFER, internal->ibl.capture_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    
    return success;
}










static bool OpenGL_Vector3NearlyEqual(Vector3 a, Vector3 b)
{
    const float epsilon = 0.0001f;
    return fabsf(a.x - b.x) < epsilon &&
           fabsf(a.y - b.y) < epsilon &&
           fabsf(a.z - b.z) < epsilon;
}










static GLReflectionProbe* OpenGL_FindOrCreateReflectionProbe(OpenGL_Backend* internal, uint32_t entity_id)
{
    GLReflectionProbe* free_slot = NULL;
    
    for (uint32_t i = 0; i < MAX_REFLECTION_PROBES; i++)
    {
        GLReflectionProbe* slot = &internal->reflection_probes[i];
        
        if (slot->active && slot->entity_id == entity_id)
            return slot;

        if (!slot->active && !free_slot)
            free_slot = slot;
    }

    if (free_slot)
    {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->active = true;
        free_slot->entity_id = entity_id;
    }

    return free_slot;
}










// Records probe results into a result struct
static void OpenGL_RecordProbeResult(OpenGL_Backend* internal, uint32_t entity_id, EnvironmentMapHandle environment, bool captured)
{
    if (internal->probe_result_count >= MAX_REFLECTION_PROBES)
        return;
    
    RenderProbeResult* out = &internal->probe_results[internal->probe_result_count++];
    out->entity_id = entity_id;
    out->environment = environment;
    out->captured = captured;
    out->dirty = !captured;
}










static void OpenGL_UpdateReflectionProbes(OpenGL_Backend* internal, uint32_t opaque_count)
{
    internal->probe_result_count = 0;

    for (uint32_t i = 0; i < MAX_REFLECTION_PROBES; i++)
        internal->reflection_probes[i].seen_this_frame = false;

    for (uint32_t i = 0; i < internal->state.reflection_probe_count; i++)
    {
        const ReflectionProbeData* data = &internal->state.reflection_probes[i];
        GLReflectionProbe* slot = OpenGL_FindOrCreateReflectionProbe(internal, data->entity_id);
        
        if (!slot)
        {
            OpenGL_RecordProbeResult(internal, data->entity_id, (EnvironmentMapHandle){0}, false);
            continue;
        }

        slot->seen_this_frame = true;
        GLEnvironmentMap* slot_env = OpenGL_GetEnvMap(internal, slot->environment);
        uint32_t source_skybox_id = internal->state.has_probe_source_env_map ? OpenGL_EnvSkyboxTextureId(internal, internal->state.probe_source_env_map) : 0;
        bool needs_capture =
            slot->captured_revision != data->revision ||
            slot->capture_resolution != data->capture_resolution ||
            slot->captured_global_skybox_id != source_skybox_id ||
            !OpenGL_Vector3NearlyEqual(slot->captured_position, data->position) ||
            slot_env == NULL || slot_env->irradiance.id == 0;

        if (!needs_capture)
        {
            OpenGL_RecordProbeResult(internal, data->entity_id, slot->environment, true);
            continue;
        }

        if (slot->environment.id != 0)
            OpenGL_ReleaseProbeEnvironment(internal, slot->environment);
        slot->environment = (EnvironmentMapHandle){0};

        GLuint timer_query = 0;
        glGenQueries(1, &timer_query);
        glBeginQuery(GL_TIME_ELAPSED, timer_query);
        bool capture_succeeded = OpenGL_CaptureReflectionProbe(internal, data, opaque_count, &slot->environment);
        glEndQuery(GL_TIME_ELAPSED);

        GLuint64 elapsed_nanoseconds = 0;
        glGetQueryObjectui64v(timer_query, GL_QUERY_RESULT, &elapsed_nanoseconds);
        glDeleteQueries(1, &timer_query);

        if (capture_succeeded)
        {
            slot->captured_revision = data->revision;
            slot->captured_position = data->position;
            slot->capture_resolution = data->capture_resolution;
            slot->captured_global_skybox_id = source_skybox_id;

            OpenGL_RecordProbeResult(internal, data->entity_id, slot->environment, true);
            
            // Uncomment to log info about probe
            // Log_Info(
            //     "Captured local IBL probe %u at %u x %u in %.2f ms",
            //     data->entity_id,
            //     data->capture_resolution,
            //     data->capture_resolution,
            //     (double)elapsed_nanoseconds / 1000000.0
            // );
        }
        else
        {
            OpenGL_RecordProbeResult(internal, data->entity_id, (EnvironmentMapHandle){0}, false);
            
            Log_Error("ERROR: Failed to capture local IBL probe %u", data->entity_id);
        }
    }

    for (uint32_t i = 0; i < MAX_REFLECTION_PROBES; i++)
    {
        GLReflectionProbe* slot = &internal->reflection_probes[i];
        if (slot->active && !slot->seen_this_frame)
        {
            OpenGL_ReleaseProbeEnvironment(internal, slot->environment);
            memset(slot, 0, sizeof(*slot));
        }
    }
}










// Renders the Skybox
void OpenGL_DrawSkybox(OpenGL_Backend* internal)
{
    uint32_t shader_id = internal->skybox.default_shader.id;
    GLEnvironmentMap* env = OpenGL_GetEnvMap(internal, internal->state.env_map);
    uint32_t tex_id = env ? env->skybox.id : 0;

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
        glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[tex_id].id);

        glUniform1f(glGetUniformLocation(prog, "u_Gamma"), internal->state.settings.gamma);
        glUniform1f(glGetUniformLocation(prog, "u_Exposure"), internal->state.settings.exposure > 0.001f ? internal->state.settings.exposure : 1.0f);
        glUniform1i(glGetUniformLocation(prog, "u_IsHDR"), env->has_ibl ? 1 : 0);

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
void OpenGL_BeginFrame(Renderer* r, const RenderView* view, const RenderLighting* lighting)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    if (!view)
        return;

    OpenGL_Resize(r, view->window_width, view->window_height);

    internal->state.view_matrix = view->view_matrix;
    internal->state.projection_matrix = view->projection_matrix;
    internal->state.camera_pos = view->camera_pos;
    internal->state.clear_flags = view->clear_flags;
    internal->state.clear_color = view->clear_color;
    internal->state.has_env_map = view->has_env_map;

    OpenGL_BindDefaultFramebuffer();
    if (view->clear_flags == RENDER_CLEAR_COLOR_AND_DEPTH)
    {
        glClearColor(view->clear_color.r, view->clear_color.g, view->clear_color.b, view->clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else if (view->clear_flags == RENDER_CLEAR_DEPTH_ONLY)
    {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // Bind the shadow map texture array to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
    glActiveTexture(GL_TEXTURE0);

    // If lighting packet does not exist, set everything to 0 and return
    if (!lighting)
    {
        internal->state.dir_light_count = 0;
        internal->state.point_light_count = 0;
        internal->state.spot_light_count = 0;
        internal->state.reflection_probe_count = 0;
        internal->probe_result_count = 0;
        internal->command_count = 0;
        internal->bone_snapshot_count = 0;
        return;
    }

    internal->state.shadow_camera_pos = lighting->shadow_camera_pos;
    internal->state.camera_forward = lighting->camera_forward;
    internal->state.camera_right = lighting->camera_right;
    internal->state.camera_up = lighting->camera_up;
    internal->state.shadow_camera_near = lighting->camera_near;
    internal->state.shadow_camera_far = lighting->camera_far;
    internal->state.shadow_camera_fov = lighting->camera_fov;
    internal->state.shadow_camera_aspect = lighting->camera_aspect;


    // Copy Directional Lights
    internal->state.dir_light_count = lighting->dir_lights ? lighting->dir_light_count : 0;
    if (internal->state.dir_light_count > MAX_DIR_LIGHTS)
        internal->state.dir_light_count = MAX_DIR_LIGHTS;
    for (uint32_t i = 0; i < internal->state.dir_light_count; i++)
        internal->state.dir_lights[i] = lighting->dir_lights[i];

    // Copy Point Lights
    internal->state.point_light_count = lighting->point_lights ? lighting->point_light_count : 0;
    if (internal->state.point_light_count > MAX_POINT_LIGHTS)
        internal->state.point_light_count = MAX_POINT_LIGHTS;
    for (uint32_t i = 0; i < internal->state.point_light_count; i++)
        internal->state.point_lights[i] = lighting->point_lights[i];
        
    // Copy Spot Lights
    internal->state.spot_light_count = lighting->spot_lights ? lighting->spot_light_count : 0;
    if (internal->state.spot_light_count > MAX_SPOT_LIGHTS)
        internal->state.spot_light_count = MAX_SPOT_LIGHTS;
    for (uint32_t i = 0; i < internal->state.spot_light_count; i++)
        internal->state.spot_lights[i] = lighting->spot_lights[i];

    internal->state.reflection_probe_count = lighting->reflection_probes ? lighting->reflection_probe_count : 0;
    if (internal->state.reflection_probe_count > MAX_REFLECTION_PROBES)
        internal->state.reflection_probe_count = MAX_REFLECTION_PROBES;

    for (uint32_t i = 0; i < internal->state.reflection_probe_count; i++)
        internal->state.reflection_probes[i] = lighting->reflection_probes[i];

    internal->state.env_map = lighting->env_map;
    internal->state.has_probe_source_env_map = lighting->has_probe_source_env_map;
    internal->state.probe_source_env_map = lighting->probe_source_env_map;
    internal->state.settings.enable_ssao = lighting->enable_ssao;
    internal->state.global_ambient_color = lighting->global_ambient_color;
    internal->state.global_ambient_illumination = lighting->global_ambient_illumination;

    if (lighting->gamma > 0.01f)
        internal->state.settings.gamma = lighting->gamma;
    else
        internal->state.settings.gamma = internal->state.settings.gamma > 0.01f ? internal->state.settings.gamma : 2.2f;

    if (lighting->exposure > 0.001f)
        internal->state.settings.exposure = lighting->exposure;
    else
        internal->state.settings.exposure = 1.0f;
    
    // Reset the queue and bone snapshot for the new frame
    internal->command_count = 0;
    internal->bone_snapshot_count = 0;
}










// Copies an item into the queue and snapshots any borrowed bone matrices.
static void OpenGL_QueueItem(OpenGL_Backend* internal, const RenderItem* item)
{
    // Return if the queue is full
    if (!item || internal->command_count >= OpenGL_MaxDrawItems(internal))
        return;
    
    RenderItem* dst = &internal->command_queue[internal->command_count++];
    *dst = *item;

    if (!dst->bone_matrices)
        return;
    
    if (internal->bone_snapshot_count >= MAX_SNAPSHOT_SKINNED)
    {
        dst->bone_matrices = NULL;
        return;
    }
    
    memcpy(internal->bone_snapshot[internal->bone_snapshot_count], dst->bone_matrices, sizeof(Matrix4) * MAX_BONES);
    dst->bone_matrices = internal->bone_snapshot[internal->bone_snapshot_count];
    internal->bone_snapshot_count++;
}










// Copies a frozen view snapshot into backend-owned storage and runs EndFrame.
void OpenGL_DrawWorld(Renderer* r, const RenderWorld* world)
{
    if (!r || !world)
        return;
    
    OpenGL_BeginFrame(r, &world->view, &world->lighting);
    
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    
    uint32_t count = world->item_count;
    if (!world->items)
        count = 0;
    uint32_t max_items = OpenGL_MaxDrawItems(internal);
    if (count > max_items)
        count = max_items;
    for (uint32_t i = 0; i < count; i++)
        OpenGL_QueueItem(internal, &world->items[i]);    
    
    OpenGL_EndFrame(r);
}










// Returns the probe results and count
uint32_t OpenGL_GetProbeResults(Renderer* r, RenderProbeResult* out, uint32_t max_count)
{
    if (!r || !r->backend_internal_data)
        return 0;

    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;
    if (!out)
        return internal->probe_result_count;
    
    uint32_t count = internal->probe_result_count;
    if (count > max_count)
        count = max_count;
    
    for (uint32_t i = 0; i < count; i++)
        out[i] = internal->probe_results[i];
    
    return count;
}










// Compare render commands (for sorting)
static int CompareRenderCommands(const void* a, const void* b)
{
    RenderItem* cmdA = (RenderItem*)a;
    RenderItem* cmdB = (RenderItem*)b;
    bool a_transparent = (cmdA->flags & RENDER_ITEM_TRANSPARENT) != 0;
    bool b_transparent = (cmdB->flags & RENDER_ITEM_TRANSPARENT) != 0;

    // Opaque commands are first
    if (a_transparent != b_transparent)
        return (int)a_transparent - (int)b_transparent;

    // If transparent, sort back to front
    if (a_transparent)
    {
        if (cmdA->depth_distance < cmdB->depth_distance) return  1;
        if (cmdA->depth_distance > cmdB->depth_distance) return -1;
        return 0;
    }

    // Sort primarily by material
    return (int)cmdA->material.id - (int)cmdB->material.id;
}










// Sorts the queue, binds the state, and executes the actual GPU draw calls
void OpenGL_EndFrame(Renderer* r)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    glViewport(0, 0, internal->state.window_width, internal->state.window_height);

    // Sort the command queue
    qsort(internal->command_queue, internal->command_count, sizeof(RenderItem), CompareRenderCommands);

    // Find where the transparent commands begin
    uint32_t transparent_start_idx = internal->command_count;
    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        if (internal->command_queue[i].flags & RENDER_ITEM_TRANSPARENT)
        {
            transparent_start_idx = i;
            break;
        }
    }

    // Shadows first so probe capture and deferred lighting use this frame's maps
    OpenGL_ExecuteShadowPass(internal);

    // Generate dirty local probes from the complete static opaque queue before the camera's normal deferred pass consumes that queue.
    OpenGL_UpdateReflectionProbes(internal, transparent_start_idx);



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

    // Depth Blit. We must copy the exact depths from the G-Buffer onto the main screen so the Skybox and Transparent objects know what to hide behind
    glBindFramebuffer(GL_READ_FRAMEBUFFER, internal->ssao.gBufferFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, internal->state.window_width, internal->state.window_height,
                      0, 0, internal->state.window_width, internal->state.window_height,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    OpenGL_BindDefaultFramebuffer();

    // Draw Skybox
    if (internal->state.has_env_map)
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