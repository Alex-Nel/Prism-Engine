#include "opengl_internal.h"
#include "../shadow_cascades.h"





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

    for (uint32_t i = 0; i < internal->command_count; i++)
    {
        RenderItem* cmd = &internal->command_queue[i];

        if (cmd->mesh.id == 0 || cmd->mesh.id >= MAX_RESOURCES || !internal->mesh_pool[cmd->mesh.id].active)
            continue;

        if ((cmd->flags & RENDER_ITEM_CAST_SHADOWS) == 0)
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

    for (uint32_t i = 0; i < opaque_count; i++)
    {
        RenderItem* cmd = &internal->command_queue[i];
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
        bool valid_albedo = (cmd->albedo.id != 0 && cmd->albedo.id < MAX_RESOURCES);
        glBindTexture(GL_TEXTURE_2D, valid_albedo ? internal->texture_pool[cmd->albedo.id].id : internal->texture_pool[1].id); // Fallback to default tex
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.albedoMap"), 0);

        // 1. Normal Map
        bool valid_normal = (cmd->normal.id != 0 && cmd->normal.id < MAX_RESOURCES && internal->texture_pool[cmd->normal.id].active && internal->texture_pool[cmd->normal.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasNormalMap"), valid_normal ? 1 : 0);
        glActiveTexture(GL_TEXTURE1);
        if (valid_normal)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->normal.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[2].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.normalMap"), 1);

        // 2. Metallic Map
        bool valid_metallic = (cmd->metallic.id != 0 && cmd->metallic.id < MAX_RESOURCES && internal->texture_pool[cmd->metallic.id].active && internal->texture_pool[cmd->metallic.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasMetallicMap"), valid_metallic ? 1 : 0);
        glActiveTexture(GL_TEXTURE2);
        if (valid_metallic)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->metallic.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[3].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.metallicMap"), 2);

        // 3. Roughness Map
        bool valid_roughness = (cmd->roughness.id != 0 && cmd->roughness.id < MAX_RESOURCES && internal->texture_pool[cmd->roughness.id].active && internal->texture_pool[cmd->roughness.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasRoughnessMap"), valid_roughness ? 1 : 0);
        glActiveTexture(GL_TEXTURE3);
        if (valid_roughness)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->roughness.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.roughnessMap"), 3);

        // 4. Ambient Occlusion Map
        bool valid_ao = (cmd->ao.id != 0 && cmd->ao.id < MAX_RESOURCES && internal->texture_pool[cmd->ao.id].active && internal->texture_pool[cmd->ao.id].id != 0);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.hasAOMap"), valid_ao ? 1 : 0);
        glActiveTexture(GL_TEXTURE4);
        if (valid_ao)
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[cmd->ao.id].id);
        else
            glBindTexture(GL_TEXTURE_2D, internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(g_prog->program, "u_Material.aoMap"), 4);
        

        glUniform3fv(glGetUniformLocation(g_prog->program, "u_Material.albedoTint"), 1, (float*)&cmd->material.albedo_tint);
        glUniform1f(glGetUniformLocation(g_prog->program, "u_Material.metallicFactor"), cmd->material.metallic_factor);
        glUniform1f(glGetUniformLocation(g_prog->program, "u_Material.roughnessFactor"), cmd->material.roughness_factor);
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

    // Bind IBL Maps
    glUniform1i(glGetUniformLocation(def_prog, "u_HasIBL"), (internal->state.has_env_map && internal->state.env_map.has_ibl) ? 1 : 0);
    const int ibl_debug_mode = 0; // Set to 1 for compressed RGB irradiance or 2 for logarithmic luminance.
    glUniform1i(glGetUniformLocation(def_prog, "u_IBLDebugMode"), ibl_debug_mode);
    if (internal->state.has_env_map)
    {
        glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[internal->state.env_map.irradiance.id].id); glUniform1i(glGetUniformLocation(def_prog, "irradianceMap"), 5);
        glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[internal->state.env_map.prefilter.id].id); glUniform1i(glGetUniformLocation(def_prog, "prefilterMap"), 6);
        glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, internal->texture_pool[internal->state.env_map.brdf_lut.id].id); glUniform1i(glGetUniformLocation(def_prog, "brdfLUT"), 7);
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

        bool has_global_ibl = internal->state.has_env_map && internal->state.env_map.has_ibl;
        glUniform1i(glGetUniformLocation(probe_program, "u_HasGlobalIBL"), has_global_ibl ? 1 : 0);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, has_global_ibl ? internal->texture_pool[internal->state.env_map.irradiance.id].id : 0);
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_CUBE_MAP, has_global_ibl ? internal->texture_pool[internal->state.env_map.prefilter.id].id : 0);
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, has_global_ibl ? internal->texture_pool[internal->state.env_map.brdf_lut.id].id : 0);

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

            if (!captured || captured->environment.irradiance.id == 0)
                continue;

            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[captured->environment.irradiance.id].id);
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[captured->environment.prefilter.id].id);

            if (!has_global_ibl && captured->environment.brdf_lut.id != 0)
            {
                glActiveTexture(GL_TEXTURE8);
                glBindTexture(GL_TEXTURE_2D, internal->texture_pool[captured->environment.brdf_lut.id].id);
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

    for (uint32_t i = start_idx; i < end_idx; i++)
    {
        RenderItem* cmd = &internal->command_queue[i];
        if (!internal->mesh_pool[cmd->mesh.id].active)
            continue;

        if (probe_capture && (cmd->flags & RENDER_ITEM_PROBE_CAPTURE) == 0)
            continue;

        ShaderHandle target_handle = probe_capture ? (ShaderHandle){0} : cmd->shader;
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


        // 0. Albedo Map
        glActiveTexture(GL_TEXTURE0);
        bool valid_albedo = (cmd->albedo.id != 0 && cmd->albedo.id < MAX_RESOURCES && internal->texture_pool[cmd->albedo.id].active && internal->texture_pool[cmd->albedo.id].id != 0);
        glBindTexture(GL_TEXTURE_2D, valid_albedo ? internal->texture_pool[cmd->albedo.id].id : internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.albedoMap"), 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.diffuse"), 0);

        // 1. Normal Map
        bool valid_normal = (cmd->normal.id != 0 && cmd->normal.id < MAX_RESOURCES && internal->texture_pool[cmd->normal.id].active && internal->texture_pool[cmd->normal.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasNormalMap"), valid_normal ? 1 : 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, valid_normal ? internal->texture_pool[cmd->normal.id].id : internal->texture_pool[2].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.normalMap"), 1);

        // 2. Metallic Map
        bool valid_metallic = (cmd->metallic.id != 0 && cmd->metallic.id < MAX_RESOURCES && internal->texture_pool[cmd->metallic.id].active && internal->texture_pool[cmd->metallic.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasMetallicMap"), valid_metallic ? 1 : 0);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, valid_metallic ? internal->texture_pool[cmd->metallic.id].id : internal->texture_pool[3].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.metallicMap"), 2);

        // 3. Roughness Map
        bool valid_roughness = (cmd->roughness.id != 0 && cmd->roughness.id < MAX_RESOURCES && internal->texture_pool[cmd->roughness.id].active && internal->texture_pool[cmd->roughness.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasRoughnessMap"), valid_roughness ? 1 : 0);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, valid_roughness ? internal->texture_pool[cmd->roughness.id].id : internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.roughnessMap"), 3);

        // 4. Ambient Occlusion Map
        bool valid_ao = (cmd->ao.id != 0 && cmd->ao.id < MAX_RESOURCES && internal->texture_pool[cmd->ao.id].active && internal->texture_pool[cmd->ao.id].id != 0);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.hasAOMap"), valid_ao ? 1 : 0);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, valid_ao ? internal->texture_pool[cmd->ao.id].id : internal->texture_pool[1].id);
        glUniform1i(glGetUniformLocation(gl_shader->program, "u_Material.aoMap"), 4);


        glUniformMatrix4fv(glGetUniformLocation(gl_shader->program, "u_Model"), 1, GL_FALSE, (float*)&cmd->transform);
        glUniform3fv(glGetUniformLocation(gl_shader->program, "u_Material.tint"), 1, (float*)&cmd->material.albedo_tint);
        glUniform1f(glGetUniformLocation(gl_shader->program, "u_Material.metallicFactor"), cmd->material.metallic_factor);
        glUniform1f(glGetUniformLocation(gl_shader->program, "u_Material.roughnessFactor"), cmd->material.roughness_factor);
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










static void OpenGL_ReleaseProbeEnvironment(OpenGL_Backend* internal, EnvironmentMap* environment)
{
    TextureHandle handles[3] = {
        environment->skybox,
        environment->irradiance,
        environment->prefilter
    };

    for (uint32_t i = 0; i < 3; i++)
    {
        uint32_t id = handles[i].id;
        if (id == 0 || id >= MAX_RESOURCES || !internal->texture_pool[id].active)
            continue;

        if (internal->texture_pool[id].id != 0)
            glDeleteTextures(1, &internal->texture_pool[id].id);

        internal->texture_pool[id].id = 0;
        internal->texture_pool[id].active = false;
    }

    memset(environment, 0, sizeof(*environment));
}










static bool OpenGL_ReserveProbeEnvironment(OpenGL_Backend* internal, EnvironmentMap* environment)
{
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

        return false;
    }

    memset(environment, 0, sizeof(*environment));
    environment->skybox = (TextureHandle){ids[0]};
    environment->irradiance = (TextureHandle){ids[1]};
    environment->prefilter = (TextureHandle){ids[2]};
    environment->brdf_lut = internal->state.has_probe_source_env_map ? internal->state.probe_source_env_map.brdf_lut : (TextureHandle){0};
    environment->has_ibl = true;

    return true;
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










static bool OpenGL_ConvolveProbeCubemap(OpenGL_Backend* internal, EnvironmentMap* environment, GLuint radiance_cubemap)
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










static bool OpenGL_CaptureReflectionProbe(OpenGL_Backend* internal, const ReflectionProbeData* probe, uint32_t opaque_count, EnvironmentMap* environment)
{
    if (internal->forward.default_shader.id == 0 || internal->ibl.irradiance_convolution.id == 0 || internal->ibl.prefilter.id == 0)
        return false;

    if (!OpenGL_ReserveProbeEnvironment(internal, environment))
        return false;

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
    internal->texture_pool[environment->skybox.id].id = radiance_cubemap;

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
            OpenGL_ReleaseProbeEnvironment(internal, environment);
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

        if (internal->state.has_probe_source_env_map &&
            internal->state.probe_source_env_map.skybox.id > 0 &&
            internal->state.probe_source_env_map.skybox.id < MAX_RESOURCES &&
            internal->texture_pool[internal->state.probe_source_env_map.skybox.id].active &&
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
            glUniform1i(glGetUniformLocation(sky_program, "u_IsHDR"), internal->state.probe_source_env_map.has_ibl ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, internal->texture_pool[internal->state.probe_source_env_map.skybox.id].id);
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
    bool success = OpenGL_ConvolveProbeCubemap(internal, environment, radiance_cubemap);

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










static void OpenGL_UpdateReflectionProbes(OpenGL_Backend* internal, uint32_t opaque_count)
{
    for (uint32_t i = 0; i < MAX_REFLECTION_PROBES; i++)
        internal->reflection_probes[i].seen_this_frame = false;

    for (uint32_t i = 0; i < internal->state.reflection_probe_count; i++)
    {
        ReflectionProbeData* data = &internal->state.reflection_probes[i];
        GLReflectionProbe* slot = OpenGL_FindOrCreateReflectionProbe(internal, data->entity_id);
        
        if (!slot)
            continue;

        slot->seen_this_frame = true;
        bool needs_capture =
            slot->captured_revision != data->revision ||
            slot->capture_resolution != data->capture_resolution ||
            slot->captured_global_skybox_id != (internal->state.has_probe_source_env_map ? internal->state.probe_source_env_map.skybox.id : 0) ||
            !OpenGL_Vector3NearlyEqual(slot->captured_position, data->position) ||
            slot->environment.irradiance.id == 0;

        if (!needs_capture)
        {
            data->environment = slot->environment;
            data->dirty = false;
            data->captured = true;
            data->needs_capture = false;

            continue;
        }

        if (slot->environment.skybox.id != 0)
            OpenGL_ReleaseProbeEnvironment(internal, &slot->environment);

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
            slot->captured_global_skybox_id = internal->state.has_probe_source_env_map ? internal->state.probe_source_env_map.skybox.id : 0;
            
            data->environment = slot->environment;
            data->dirty = false;
            data->captured = true;
            data->needs_capture = false;
            
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
            memset(&data->environment, 0, sizeof(data->environment));
            data->dirty = true;
            data->captured = false;
            data->needs_capture = true;
            
            Log_Error("ERROR: Failed to capture local IBL probe %u", data->entity_id);
        }
    }

    for (uint32_t i = 0; i < MAX_REFLECTION_PROBES; i++)
    {
        GLReflectionProbe* slot = &internal->reflection_probes[i];
        if (slot->active && !slot->seen_this_frame)
        {
            OpenGL_ReleaseProbeEnvironment(internal, &slot->environment);
            memset(slot, 0, sizeof(*slot));
        }
    }
}










// Renders the Skybox
void OpenGL_DrawSkybox(OpenGL_Backend* internal)
{
    uint32_t shader_id = internal->skybox.default_shader.id;
    uint32_t tex_id = internal->state.env_map.skybox.id;

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
        glUniform1i(glGetUniformLocation(prog, "u_IsHDR"), internal->state.env_map.has_ibl ? 1 : 0);

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
    internal->state.shadow_camera_pos = packet->shadow_camera_pos;
    internal->state.camera_forward = packet->camera_forward;
    internal->state.camera_right = packet->camera_right;
    internal->state.camera_up = packet->camera_up;
    internal->state.shadow_camera_near = packet->camera_near;
    internal->state.shadow_camera_far = packet->camera_far;
    internal->state.shadow_camera_fov = packet->camera_fov;
    internal->state.shadow_camera_aspect = packet->camera_aspect;
    internal->state.clear_flags = packet->clear_flags;
    internal->state.clear_color = packet->clear_color;

    OpenGL_BindDefaultFramebuffer();
    if (packet->clear_flags == RENDER_CLEAR_COLOR_AND_DEPTH)
    {
        glClearColor(packet->clear_color.r, packet->clear_color.g, packet->clear_color.b, packet->clear_color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    else if (packet->clear_flags == RENDER_CLEAR_DEPTH_ONLY)
    {
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    // Bind the shadow map texture array to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, internal->shadow.depthMapTextureArray);
    glActiveTexture(GL_TEXTURE0);

    // Copy Directional Lights
    internal->state.dir_light_count = packet->dir_light_count;
    if (internal->state.dir_light_count > MAX_DIR_LIGHTS)
        internal->state.dir_light_count = MAX_DIR_LIGHTS;
    for (uint32_t i = 0; i < internal->state.dir_light_count; i++)
        internal->state.dir_lights[i] = packet->dir_lights[i];

    // Copy Point Lights
    internal->state.point_light_count = packet->point_light_count;
    if (internal->state.point_light_count > MAX_POINT_LIGHTS)
        internal->state.point_light_count = MAX_POINT_LIGHTS;
    for (uint32_t i = 0; i < internal->state.point_light_count; i++)
        internal->state.point_lights[i] = packet->point_lights[i];
        
    // Copy Spot Lights
    internal->state.spot_light_count = packet->spot_light_count;
    if (internal->state.spot_light_count > MAX_SPOT_LIGHTS)
        internal->state.spot_light_count = MAX_SPOT_LIGHTS;
    for (uint32_t i = 0; i < internal->state.spot_light_count; i++)
        internal->state.spot_lights[i] = packet->spot_lights[i];

    internal->state.reflection_probe_count = packet->reflection_probe_count;
    if (internal->state.reflection_probe_count > MAX_REFLECTION_PROBES)
        internal->state.reflection_probe_count = MAX_REFLECTION_PROBES;

    internal->state.reflection_probe_results = packet->reflection_probes;
    for (uint32_t i = 0; i < internal->state.reflection_probe_count; i++)
        internal->state.reflection_probes[i] = packet->reflection_probes[i];

    internal->state.has_env_map = packet->has_env_map;
    internal->state.env_map = packet->env_map;
    internal->state.has_probe_source_env_map = packet->has_probe_source_env_map;
    internal->state.probe_source_env_map = packet->probe_source_env_map;
    internal->state.settings.enable_ssao = packet->enable_ssao;
    internal->state.global_ambient_color = packet->global_ambient_color;
    internal->state.global_ambient_illumination = packet->global_ambient_illumination;

    if (packet->gamma > 0.01f)
        internal->state.settings.gamma = packet->gamma;
    else
        internal->state.settings.gamma = internal->state.settings.gamma > 0.01f ? internal->state.settings.gamma : 2.2f;

    if (packet->exposure > 0.001f)
        internal->state.settings.exposure = packet->exposure;
    else
        internal->state.settings.exposure = 1.0f;
    
    // Reset the queue for the new frame
    internal->command_count = 0;
}










// Adds an object to the draw queue
void OpenGL_Submit(Renderer* r, const RenderItem* item)
{
    OpenGL_Backend* internal = (OpenGL_Backend*)r->backend_internal_data;

    // Return if the queue is full
    if (!item || internal->command_count >= MAX_COMMANDS)
        return;
    
    internal->command_queue[internal->command_count++] = *item;
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

    // Sort primarily by shader, then by texture
    if (cmdA->shader.id != cmdB->shader.id)
        return (int)cmdA->shader.id - (int)cmdB->shader.id;
    
    return (int)cmdA->albedo.id - (int)cmdB->albedo.id;
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

    if (internal->state.reflection_probe_results)
    {
        for (uint32_t i = 0; i < internal->state.reflection_probe_count; i++)
        {
            internal->state.reflection_probe_results[i].environment = internal->state.reflection_probes[i].environment;
            internal->state.reflection_probe_results[i].dirty = internal->state.reflection_probes[i].dirty;
            internal->state.reflection_probe_results[i].captured = internal->state.reflection_probes[i].captured;
            internal->state.reflection_probe_results[i].needs_capture = internal->state.reflection_probes[i].needs_capture;
        }
        
        internal->state.reflection_probe_results = NULL;
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