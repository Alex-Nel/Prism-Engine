#include "engine_runtime.h"



// Sorting function for cameras (lowest order renders first)
static int CompareCameraOrder(const void* a, const void* b)
{
    return ((ActiveCamera*)a)->render_order - ((ActiveCamera*)b)->render_order;
}





// Compares probe priority
static int CompareProbePriority(const void* a, const void* b)
{
    const ReflectionProbeData* probe_a = (const ReflectionProbeData*)a;
    const ReflectionProbeData* probe_b = (const ReflectionProbeData*)b;

    if (probe_a->priority == probe_b->priority)
        return 0;

    return probe_a->priority > probe_b->priority ? -1 : 1;
}





// Gathers all the lights in a scene and puts them in a render packet
void Engine_GatherSceneLights(PrismEngine* engine, Scene* scene, RenderPacket* packet, DirectionalLightData* dir_lights, PointLightData* point_lights, SpotLightData* spot_lights)
{
    uint32_t dir_count = 0, point_count = 0, spot_count = 0;
    uint32_t light_mask = COMPONENT_TRANSFORM | COMPONENT_LIGHT;
    float pi = 3.14159265359f;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & light_mask) != light_mask)
            continue;

        Transform* t = &scene->transforms[i];
        LightComponent* l = &scene->lights[i];

        if (!l->is_active)
            continue;

        if (l->type == LIGHT_DIRECTIONAL && dir_count < MAX_RESOURCES)
        {
            dir_lights[dir_count].direction = Transform_GetForwardVector(t);
            dir_lights[dir_count].color = l->color;
            dir_lights[dir_count].intensity = l->intensity;
            dir_lights[dir_count].ambient_strength = l->ambient_strength;
            dir_lights[dir_count].shadow_box_size = l->shadow_box_size;
            dir_lights[dir_count].shadow_cascade_count = l->shadow_cascade_count;
            dir_lights[dir_count].shadow_max_distance = l->shadow_max_distance;
            dir_lights[dir_count].cascade_split_lambda = l->cascade_split_lambda;
            dir_lights[dir_count].cascade_blend_fraction = l->cascade_blend_fraction;
            dir_lights[dir_count].casts_shadows = l->casts_shadows;
            dir_count++;
        }
        else if (l->type == LIGHT_POINT && point_count < MAX_RESOURCES)
        {
            point_lights[point_count].position = Transform_GetGlobalPosition(t);
            point_lights[point_count].color = l->color;
            point_lights[point_count].intensity = l->intensity;
            point_lights[point_count].constant = l->constant;
            point_lights[point_count].linear = l->linear;
            point_lights[point_count].quadratic = l->quadratic;
            point_lights[point_count].casts_shadows = l->casts_shadows;
            point_count++;
        }
        else if (l->type == LIGHT_SPOT && spot_count < MAX_RESOURCES)
        {
            spot_lights[spot_count].position = Transform_GetGlobalPosition(t);
            spot_lights[spot_count].direction = Transform_GetForwardVector(t);
            spot_lights[spot_count].color = l->color;
            spot_lights[spot_count].intensity = l->intensity;
            spot_lights[spot_count].constant = l->constant;
            spot_lights[spot_count].linear = l->linear;
            spot_lights[spot_count].quadratic = l->quadratic;
            spot_lights[spot_count].inner_cut_off = cosf(l->inner_cut_off * (pi / 180.0f));
            spot_lights[spot_count].outer_cut_off = cosf(l->outer_cut_off * (pi / 180.0f));
            spot_lights[spot_count].casts_shadows = l->casts_shadows;
            spot_count++;
        }
    }

    packet->dir_lights = dir_lights;
    packet->dir_light_count = dir_count;
    packet->point_lights = point_lights;
    packet->point_light_count = point_count;
    packet->spot_lights = spot_lights;
    packet->spot_light_count = spot_count;
}










// Gathers active local IBL probes as value data. Capture results are copied back explicitly after rendering
void Engine_GatherReflectionProbes(PrismEngine* engine, Scene* scene, RenderPacket* packet, ReflectionProbeData* probes)
{
    uint32_t count = 0;
    const uint32_t required_mask = COMPONENT_TRANSFORM | COMPONENT_REFLECTION_PROBE;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & required_mask) != required_mask)
            continue;

        ReflectionProbeComponent* probe = &scene->reflection_probes[i];
        if (!probe->is_active)
        {
            probe->dirty = true;
            probe->captured = false;
            memset(&probe->environment, 0, sizeof(probe->environment));
            continue;
        }

        Transform* transform = &scene->transforms[i];
        Vector3 world_scale = Transform_GetGlobalScale(transform);
        Vector3 world_position = Transform_GetGlobalPosition(transform);
        if (probe->captured && (fabsf(world_position.x - probe->last_capture_position.x) > 0.0001f ||
                                fabsf(world_position.y - probe->last_capture_position.y) > 0.0001f ||
                                fabsf(world_position.z - probe->last_capture_position.z) > 0.0001f))
        {
            ReflectionProbe_MarkDirty(probe);
        }

        ReflectionProbeData candidate = {0};

        candidate.entity_id = i;
        candidate.position = world_position;
        candidate.box_extents = (Vector3){
            fabsf(probe->box_extents.x * world_scale.x),
            fabsf(probe->box_extents.y * world_scale.y),
            fabsf(probe->box_extents.z * world_scale.z)
        };
        candidate.blend_distance = probe->blend_distance;
        candidate.priority = probe->priority;
        candidate.capture_resolution = probe->capture_resolution;
        candidate.revision = probe->revision;
        candidate.needs_capture = probe->dirty || !probe->captured;
        candidate.environment = probe->environment;
        candidate.dirty = probe->dirty;
        candidate.captured = probe->captured;

        if (count < MAX_REFLECTION_PROBES)
        {
            probes[count++] = candidate;
        }
        else
        {
            uint32_t lowest_index = 0;
            for (uint32_t slot = 1; slot < count; slot++)
            {
                if (probes[slot].priority < probes[lowest_index].priority)
                    lowest_index = slot;
            }

            if (candidate.priority > probes[lowest_index].priority)
                probes[lowest_index] = candidate;
        }
    }

    packet->reflection_probes = probes;
    packet->reflection_probe_count = count;

    qsort(probes, count, sizeof(ReflectionProbeData), CompareProbePriority);

    for (uint32_t i = 0; i < count; i++)
    {
        ReflectionProbeComponent* selected = &scene->reflection_probes[probes[i].entity_id];
        selected->last_capture_position = probes[i].position;
    }
}










// Applies reflection probe changes from the renderer back to the components
void Engine_ApplyReflectionProbeResults(PrismEngine* engine, Scene* scene, const ReflectionProbeData* probes, uint32_t probe_count)
{
    for (uint32_t i = 0; i < probe_count; i++)
    {
        uint32_t entity_id = probes[i].entity_id;
        if (entity_id >= MAX_ENTITIES || !(scene->component_masks[entity_id] & COMPONENT_REFLECTION_PROBE))
            continue;

        ReflectionProbeComponent* component = &scene->reflection_probes[entity_id];
        component->environment = probes[i].environment;
        component->dirty = probes[i].dirty;
        component->captured = probes[i].captured;
    }
}










// Executes the shadow pass of the rendering pipeline with a rendering packet
void Engine_ExecuteShadowPass(PrismEngine* engine, Scene* scene, RenderPacket* packet)
{
    DirectionalLightData* shadow_light = &packet->dir_lights[0];
    Vector3 light_dir = shadow_light->direction;

    Transform* main_cam_t = &scene->transforms[scene->main_camera_id];
    CameraComponent* main_cam = &scene->cameras[scene->main_camera_id];
    Vector3 cam_pos = Transform_GetGlobalPosition(main_cam_t);
    Vector3 cam_fwd = Transform_GetForwardVector(main_cam_t);
    Vector3 cam_right = Transform_GetRightVector(main_cam_t);
    Vector3 cam_up = Transform_GetUpVector(main_cam_t);

    packet->camera_forward = cam_fwd;

    Camera_RecalculateProjectionIfNeeded(main_cam);
    
    packet->shadow_camera_near = main_cam->nearZ;
    packet->cascade_blend_fraction = shadow_light->cascade_blend_fraction;
    
    if (packet->cascade_blend_fraction <= 0.0f)
        packet->cascade_blend_fraction = 0.12f;
    
    if (packet->cascade_blend_fraction > 0.5f)
        packet->cascade_blend_fraction = 0.5f;

    float light_distance = 80.0f;
    Vector3 up;
    if (fabsf(light_dir.y) > 0.99f)
        up = (Vector3){0.0f, 0.0f, 1.0f};
    else
        up = (Vector3){0.0f, 1.0f, 0.0f};

    uint32_t cascade_count = shadow_light->shadow_cascade_count;
    if (cascade_count < 1)
        cascade_count = 1;
    if (cascade_count > MAX_SHADOW_CASCADES)
        cascade_count = MAX_SHADOW_CASCADES;
    
    packet->shadow_cascade_count = cascade_count;


    // If the cascade count is 1 or 0, use only one shadow map
    if (cascade_count <= 1)
    {
        float shadow_box_size = shadow_light->shadow_box_size;
        if (shadow_box_size <= 0.0f)
            shadow_box_size = 20.0f;

        Vector3 center = {
            cam_pos.x + cam_fwd.x * shadow_box_size * 0.5f,
            cam_pos.y + cam_fwd.y * shadow_box_size * 0.5f,
            cam_pos.z + cam_fwd.z * shadow_box_size * 0.5f
        };
        RendererSettings cur_settings = Render_GetSettings(engine->renderer);
        float cur_shadow_res = (float)SHADOW_MAP_RESOLUTION;
        if (cur_settings.shadow_map_resolution > 0)
            cur_shadow_res = (float)cur_settings.shadow_map_resolution;
        float texel_world_size = (2.0f * shadow_box_size) / cur_shadow_res;

        Matrix4 light_basis = Matrix4LookAt((Vector3){0.0f, 0.0f, 0.0f}, light_dir, up);
        Vector4 center_ls = Matrix4MultiplyVector4(light_basis, (Vector4){center.x, center.y, center.z, 1.0f});
        center_ls.x = floorf(center_ls.x / texel_world_size) * texel_world_size;
        center_ls.y = floorf(center_ls.y / texel_world_size) * texel_world_size;

        Vector4 snapped = Matrix4MultiplyVector4(Matrix4Transpose(light_basis), center_ls);
        Vector3 center_snapped = { snapped.x, snapped.y, snapped.z };

        Vector3 light_pos = {
            center_snapped.x - light_dir.x * light_distance,
            center_snapped.y - light_dir.y * light_distance,
            center_snapped.z - light_dir.z * light_distance
        };

        Matrix4 light_view = Matrix4LookAt(light_pos, center_snapped, up);
        Matrix4 light_proj = Matrix4Ortho(-shadow_box_size, shadow_box_size,
                                          -shadow_box_size, shadow_box_size,
                                          0.1f, 2.0f * light_distance);

        packet->light_space_matrices[0] = Matrix4Multiply(light_proj, light_view);
        packet->shadow_texel_world_sizes[0] = texel_world_size;
    }
    // Separate shadow map into specified number of shadow cascades
    else
    {
        float aspect = (float)main_cam->viewport_width / (float)main_cam->viewport_height;
        if (aspect <= 0.0f)
            aspect = 1.0f;

        float cam_near = main_cam->nearZ;
        float shadow_far = shadow_light->shadow_max_distance;
        if (shadow_far <= cam_near)
            shadow_far = cam_near + 1.0f;
        if (shadow_far > main_cam->farZ)
            shadow_far = main_cam->farZ;

        float split_lambda = shadow_light->cascade_split_lambda;
        if (split_lambda < 0.0f)
            split_lambda = 0.0f;
        if (split_lambda > 1.0f)
            split_lambda = 1.0f;

        float splits[MAX_SHADOW_CASCADES - 1];
        for (uint32_t i = 1; i < cascade_count; i++)
        {
            float p = (float)i / (float)cascade_count;
            float log_split = cam_near * powf(shadow_far / cam_near, p);
            float uni_split = cam_near + (shadow_far - cam_near) * p;
            splits[i - 1] = uni_split * (1.0f - split_lambda) + log_split * split_lambda;
            packet->cascade_splits[i - 1] = splits[i - 1];
        }

        // cam->fov is stored in radians
        float tan_half = tanf(main_cam->fov * 0.5f);

        for (uint32_t c = 0; c < cascade_count; c++)
        {
            float split_near;
            if (c == 0)
                split_near = cam_near;
            else
                split_near = splits[c - 1];

            float split_far;
            if (c == cascade_count - 1)
                split_far = shadow_far;
            else
                split_far = splits[c];

            if (c > 0)
            {
                float prev_near;
                if (c == 1)
                    prev_near = cam_near;
                else
                    prev_near = splits[c - 2];

                float prev_slice = splits[c - 1] - prev_near;
                split_near -= prev_slice * packet->cascade_blend_fraction;
                if (split_near < cam_near)
                    split_near = cam_near;
            }

            Vector3 corners[8];
            BuildFrustumSliceCorners(cam_pos, cam_fwd, cam_right, cam_up, aspect, tan_half, split_near, split_far, corners);


            // --- Texel Snapping for cascades ---
            
            // Find the center of the frustum slice
            Vector3 center = {0,0,0};
            for (int k = 0; k < 8; k++) {
                center.x += corners[k].x;
                center.y += corners[k].y;
                center.z += corners[k].z;
            }
            center.x /= 8.0f; center.y /= 8.0f; center.z /= 8.0f;

            // Find the bounding sphere radius to keep the box size stable during camera rotation
            float radius = 0.0f;
            for (int k = 0; k < 8; k++) {
                float dx = corners[k].x - center.x;
                float dy = corners[k].y - center.y;
                float dz = corners[k].z - center.z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                if (dist > radius) radius = dist;
            }
            
            // Round radius up to nearest 16 units to prevent micro-fluctuations
            radius = ceilf(radius / 16.0f) * 16.0f;
            
            float shadow_box_size = radius; 
            RendererSettings cur_settings = Render_GetSettings(engine->renderer);
            float cur_shadow_res = (float)SHADOW_MAP_RESOLUTION;
            if (cur_settings.shadow_map_resolution > 0)
                cur_shadow_res = (float)cur_settings.shadow_map_resolution;
            float texel_world_size = (2.0f * shadow_box_size) / cur_shadow_res;

            // Snap the center to the texel grid
            Matrix4 light_basis = Matrix4LookAt((Vector3){0.0f, 0.0f, 0.0f}, light_dir, up);
            Vector4 center_ls = Matrix4MultiplyVector4(light_basis, (Vector4){center.x, center.y, center.z, 1.0f});
            center_ls.x = floorf(center_ls.x / texel_world_size) * texel_world_size;
            center_ls.y = floorf(center_ls.y / texel_world_size) * texel_world_size;

            Vector4 snapped = Matrix4MultiplyVector4(Matrix4Transpose(light_basis), center_ls);
            Vector3 center_snapped = { snapped.x, snapped.y, snapped.z };

            Vector3 light_pos = {
                center_snapped.x - light_dir.x * light_distance,
                center_snapped.y - light_dir.y * light_distance,
                center_snapped.z - light_dir.z * light_distance
            };

            Matrix4 light_view = Matrix4LookAt(light_pos, center_snapped, up);
            Matrix4 light_proj = Matrix4Ortho(-shadow_box_size, shadow_box_size, -shadow_box_size, shadow_box_size, 0.1f, 2.0f * light_distance);

            packet->light_space_matrices[c] = Matrix4Multiply(light_proj, light_view);
            packet->shadow_texel_world_sizes[c] = texel_world_size;



            // --- UNUSED (above method seems to work better) ---
            // ComputeCascadeLightMatrix(corners, light_dir, up, light_distance, &packet->light_space_matrices[c], &packet->shadow_texel_world_sizes[c]);
        }
    }

    // Begin Shadow Pass
    Render_BeginShadowPass(engine->renderer, packet);

    float cull_dist = shadow_light->shadow_max_distance + 50.0f; 
    float cull_dist_sq = cull_dist * cull_dist;


    // Submit shadows for Static Meshes
    uint32_t req_mesh_mask = COMPONENT_TRANSFORM | COMPONENT_MESH_RENDERER;
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & req_mesh_mask) != req_mesh_mask)
            continue;

        MeshRendererComponent* rc = &scene->mesh_renderers[i];
        if (!rc->is_active || !rc->mesh)
            continue;

        Transform* t = &scene->transforms[i];
        Vector3 obj_pos = Transform_GetGlobalPosition(t);
        float dx = obj_pos.x - cam_pos.x;
        float dy = obj_pos.y - cam_pos.y;
        float dz = obj_pos.z - cam_pos.z;
        if ((dx*dx + dy*dy + dz*dz) > cull_dist_sq)
            continue; // Too far to cast a visible shadow

        Render_Submit(engine->renderer, rc->mesh->gpu_handle, DEFAULT_SHADER,
                      DEFAULT_TEXTURE, DEFAULT_TEXTURE, DEFAULT_TEXTURE, DEFAULT_TEXTURE, DEFAULT_TEXTURE,
                      (MaterialProperties){0}, t->world_matrix, NULL,
                      false, 0.0f, rc->casts_shadows, rc->receives_shadows, false);
    }


    // Submit shadows for Skinned Meshes
    uint32_t req_skin_mask = COMPONENT_TRANSFORM | COMPONENT_SKINNED_MESH_RENDERER;
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & req_skin_mask) != req_skin_mask)
            continue;

        SkinnedMeshRendererComponent* rc = &scene->skinned_mesh_renderers[i];
        if (!rc->is_active || !rc->mesh)
            continue;

        Transform* t = &scene->transforms[i];

        Vector3 obj_pos = Transform_GetGlobalPosition(t);
        float dx = obj_pos.x - cam_pos.x;
        float dy = obj_pos.y - cam_pos.y;
        float dz = obj_pos.z - cam_pos.z;
        if ((dx*dx + dy*dy + dz*dz) > cull_dist_sq)
            continue; // Too far to cast a visible shadow

        Matrix4* bone_ptr = NULL;
        uint32_t anim_id = rc->root_animator_entity_id;

        if (anim_id != 0 && anim_id != ENTITY_NONE && (scene->component_masks[anim_id] & COMPONENT_ANIMATOR))
            bone_ptr = scene->animators[anim_id].final_bone_matrices;
        else if (scene->component_masks[i] & COMPONENT_ANIMATOR)
            bone_ptr = scene->animators[i].final_bone_matrices;

        Render_Submit(engine->renderer, rc->mesh->gpu_handle, DEFAULT_SHADER,
                      DEFAULT_TEXTURE, DEFAULT_TEXTURE, DEFAULT_TEXTURE, DEFAULT_TEXTURE, DEFAULT_TEXTURE,
                      (MaterialProperties){0}, t->world_matrix, bone_ptr,
                      false, 0.0f, rc->casts_shadows, rc->receives_shadows, false);
    }

    Render_EndShadowPass(engine->renderer);
}










// Gathers and sorts all cameras in a scene
uint32_t Engine_GatherAndSortCameras(PrismEngine* engine, Scene* scene, ActiveCamera* active_cameras)
{
    uint32_t camera_count = 0;
    uint32_t cam_mask = COMPONENT_TRANSFORM | COMPONENT_CAMERA;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->is_active_in_hierarchy[i] && (scene->component_masks[i] & cam_mask) == cam_mask)
        {
            if (scene->cameras[i].is_active)
            {
                active_cameras[camera_count].entity_id = i;
                active_cameras[camera_count].render_order = scene->cameras[i].render_order;
                camera_count++;
            }
        }
    }

    qsort(active_cameras, camera_count, sizeof(ActiveCamera), CompareCameraOrder);
    
    return camera_count;
}










// Submits all visible geometry to the renderer
void Engine_SubmitVisibleGeometry(PrismEngine* engine, Scene* scene, Frustum* cam_frustum, Vector3 cam_pos, uint32_t culling_masks)
{
    // --- Static Meshes ---
    uint32_t req_mesh_mask = COMPONENT_TRANSFORM | COMPONENT_MESH_RENDERER;
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & req_mesh_mask) != req_mesh_mask)
            continue;
        
        MeshRendererComponent* rc = &scene->mesh_renderers[i];
        if (!rc->is_active || !rc->mesh || !rc->material || (rc->layer_mask & culling_masks) == 0)
            continue;

        Transform* t = &scene->transforms[i];
        if (cam_frustum && !Frustum_ContainsAABB(cam_frustum, rc->mesh->local_bounds, t->world_matrix))
            continue;

        ShaderHandle shader = DEFAULT_SHADER;
        if (rc->material->shader != NULL)
            shader = rc->material->shader->gpu_handle;

        TextureHandle albedo = rc->material->albedo_texture ? rc->material->albedo_texture->gpu_handle : (TextureHandle){0};
        TextureHandle normal = rc->material->normal_map ? rc->material->normal_map->gpu_handle : (TextureHandle){0};
        TextureHandle metallic = rc->material->metallic_map ? rc->material->metallic_map->gpu_handle : (TextureHandle){0};
        TextureHandle roughness = rc->material->roughness_map ? rc->material->roughness_map->gpu_handle : (TextureHandle){0};
        TextureHandle ao = rc->material->ao_map ? rc->material->ao_map->gpu_handle : (TextureHandle){0};
        
        // Ensure only static geometry gets included in probe captures (i.e. no physics, animations or custom scripts)
        bool include_in_probe_capture = (scene->component_masks[i] & (COMPONENT_RIGIDBODY | COMPONENT_SCRIPT | COMPONENT_ANIMATOR)) == 0;

        Render_Submit(engine->renderer, rc->mesh->gpu_handle, shader,
                      albedo, normal, metallic, roughness, ao, rc->material->properties,
                      t->world_matrix, NULL, false, 0.0f, rc->casts_shadows, rc->receives_shadows, include_in_probe_capture);
    }



    // --- Skinned Meshes ---
    uint32_t req_skin_mask = COMPONENT_TRANSFORM | COMPONENT_SKINNED_MESH_RENDERER;
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & req_skin_mask) != req_skin_mask)
            continue;

        SkinnedMeshRendererComponent* rc = &scene->skinned_mesh_renderers[i];
        if (!rc->is_active || !rc->mesh || !rc->material || (rc->layer_mask & culling_masks) == 0)
            continue;

        Transform* t = &scene->transforms[i];
        if (cam_frustum && !Frustum_ContainsAABB(cam_frustum, rc->pose_bounds, t->world_matrix))
            continue;

        Matrix4* bone_ptr = NULL;
        uint32_t anim_id = rc->root_animator_entity_id;

        if (anim_id != 0 && anim_id != ENTITY_NONE && (scene->component_masks[anim_id] & COMPONENT_ANIMATOR))
            bone_ptr = scene->animators[anim_id].final_bone_matrices;
        else if (scene->component_masks[i] & COMPONENT_ANIMATOR)
            bone_ptr = scene->animators[i].final_bone_matrices;

        ShaderHandle shader = DEFAULT_SHADER;
        if (rc->material->shader != NULL)
            shader = rc->material->shader->gpu_handle;

        TextureHandle albedo = rc->material->albedo_texture ? rc->material->albedo_texture->gpu_handle : (TextureHandle){0};
        TextureHandle normal = rc->material->normal_map ? rc->material->normal_map->gpu_handle : (TextureHandle){0};
        TextureHandle metallic = rc->material->metallic_map ? rc->material->metallic_map->gpu_handle : (TextureHandle){0};
        TextureHandle roughness = rc->material->roughness_map ? rc->material->roughness_map->gpu_handle : (TextureHandle){0};
        TextureHandle ao = rc->material->ao_map ? rc->material->ao_map->gpu_handle : (TextureHandle){0};
        
        Render_Submit(engine->renderer, rc->mesh->gpu_handle, shader,
                      albedo, normal, metallic, roughness, ao, rc->material->properties,
                      t->world_matrix, bone_ptr, false, 0.0f, rc->casts_shadows, rc->receives_shadows, false);
    }



    // --- Line Renderers ---
    uint32_t req_line_mask = COMPONENT_TRANSFORM | COMPONENT_LINE_RENDERER;
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & req_line_mask) != req_line_mask)
            continue;
        
        LineRendererComponent* line = &scene->line_renderers[i];
        if (!line->is_active || line->point_count < 2 || !line->dynamic_mesh || !line->material)
            continue;

        MaterialProperties local_props = line->material->properties;
        local_props.albedo_tint = line->color;

        ShaderHandle shader = DEFAULT_SHADER;
        if (line->material->shader != NULL)
            shader = line->material->shader->gpu_handle;

        TextureHandle albedo = line->material->albedo_texture ? line->material->albedo_texture->gpu_handle : (TextureHandle){0};
        TextureHandle normal = line->material->normal_map ? line->material->normal_map->gpu_handle : (TextureHandle){0};
        TextureHandle metallic = line->material->metallic_map ? line->material->metallic_map->gpu_handle : (TextureHandle){0};
        TextureHandle roughness = line->material->roughness_map ? line->material->roughness_map->gpu_handle : (TextureHandle){0};
        TextureHandle ao = line->material->ao_map ? line->material->ao_map->gpu_handle : (TextureHandle){0};

        Render_Submit(engine->renderer, line->dynamic_mesh->gpu_handle, shader,
                      albedo, normal, metallic, roughness, ao,
                      local_props, Matrix4Identity(), NULL, false, 0.0f, false, false, false);
    }



    // --- Sprite Renderers ---
    uint32_t req_sprite_mask = COMPONENT_TRANSFORM | COMPONENT_SPRITE_RENDERER;
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i] || (scene->component_masks[i] & req_sprite_mask) != req_sprite_mask)
            continue;
        
        SpriteRendererComponent* sprite = &scene->sprite_renderers[i];
        if (!sprite->is_active || !sprite->quad || !sprite->material)
            continue;

        Transform* t = &scene->transforms[i];

        Vector3 sprite_pos = Transform_GetGlobalPosition(t);
        float dx = cam_pos.x - sprite_pos.x;
        float dy = cam_pos.y - sprite_pos.y;
        float dz = cam_pos.z - sprite_pos.z;
        float dist_sq = (dx * dx) + (dy * dy) + (dz * dz);

        MaterialProperties local_props = sprite->material->properties;
        local_props.albedo_tint = sprite->color;

        float aspect_x = (float)sprite->material->albedo_texture->width / (float)sprite->material->albedo_texture->height;
        Matrix4 aspect_matrix = Matrix4Identity();
        aspect_matrix.m00 = aspect_x;
        
        Matrix4 final_sprite_matrix = Matrix4Multiply(t->world_matrix, aspect_matrix);

        ShaderHandle shader = DEFAULT_SHADER;
        if (sprite->material->shader != NULL)
            shader = sprite->material->shader->gpu_handle;

        TextureHandle albedo = sprite->material->albedo_texture ? sprite->material->albedo_texture->gpu_handle : (TextureHandle){0};
        TextureHandle normal = sprite->material->normal_map ? sprite->material->normal_map->gpu_handle : (TextureHandle){0};
        TextureHandle metallic = sprite->material->metallic_map ? sprite->material->metallic_map->gpu_handle : (TextureHandle){0};
        TextureHandle roughness = sprite->material->roughness_map ? sprite->material->roughness_map->gpu_handle : (TextureHandle){0};
        TextureHandle ao = sprite->material->ao_map ? sprite->material->ao_map->gpu_handle : (TextureHandle){0};

        Render_Submit(engine->renderer, sprite->quad->gpu_handle, shader,
                      albedo, normal, metallic, roughness, ao,
                      local_props, final_sprite_matrix, NULL, true, dist_sq, false, false, false);
    }
}










// Renders a specified scene
void Engine_RenderScene(PrismEngine* engine, Scene* scene)
{
    if (!scene)
        return;

    uint32_t win_w = Platform_GetWindowWidth(engine->window);
    uint32_t win_h = Platform_GetWindowHeight(engine->window);

    CameraComponent* cam = &scene->cameras[scene->main_camera_id];
    if (win_w > 0 && win_h > 0)
    {
        cam->viewport_x = 0;
        cam->viewport_y = 0;
        cam->viewport_width = win_w;
        cam->viewport_height = win_h;
    }

    // Make an empty render packet to send to the renderer
    RenderPacket packet = {0};
    RendererSettings cur_settings = Render_GetSettings(engine->renderer);
    packet.enable_ssao = cur_settings.enable_ssao;
    packet.global_ambient_color = scene->ambient_color;
    packet.global_ambient_illumination = scene->ambient_illumination;
    packet.exposure = scene->exposure;
    packet.gamma = 2.2f;
    if (cur_settings.gamma > 0.01f)
        packet.gamma = cur_settings.gamma;
    
    packet.has_env_map = scene->has_env_map;
    packet.has_probe_source_env_map = scene->has_env_map;

    if (scene->has_env_map && scene->env_map)
    {
        packet.env_map = *scene->env_map;
        packet.probe_source_env_map = *scene->env_map;
    }

    // --- Get all Point Lights from the ECS ---
    DirectionalLightData active_dir_lights[MAX_RESOURCES];
    PointLightData active_point_lights[MAX_RESOURCES];
    SpotLightData active_spot_lights[MAX_RESOURCES];
    ReflectionProbeData active_reflection_probes[MAX_REFLECTION_PROBES];


    // Fille the packet with all the lights in the scene
    Engine_GatherSceneLights(engine, scene, &packet, active_dir_lights, active_point_lights, active_spot_lights);
    Engine_GatherReflectionProbes(engine, scene, &packet, active_reflection_probes);
    
    bool probe_capture_requested = false;
    for (uint32_t i = 0; i < packet.reflection_probe_count; i++)
    {
        if (packet.reflection_probes[i].needs_capture)
        {
            probe_capture_requested = true;
            break;
        }
    }

    // If there are directional lights, render all shadows
    if (packet.dir_light_count > 0)
        Engine_ExecuteShadowPass(engine, scene, &packet);
    

    // Gather and sort cameras
    ActiveCamera active_cameras[MAX_ENTITIES];
    uint32_t camera_count = Engine_GatherAndSortCameras(engine, scene, active_cameras);


    // Execute render pass per camera
    for (uint32_t c = 0; c < camera_count; c++)
    {
        uint32_t cam_id = active_cameras[c].entity_id;
        Transform* cam_transform = &scene->transforms[cam_id];
        CameraComponent* cam_comp = &scene->cameras[cam_id];

        // Keep every cameras viewport in sync with the current window size
        if (win_w > 0 && win_h > 0)
        {
            cam_comp->viewport_x = 0;
            cam_comp->viewport_y = 0;
            cam_comp->viewport_width = win_w;
            cam_comp->viewport_height = win_h;
        }

        // Clear Screen
        if (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH)
        {
            Color bg = scene->background_color;
            Render_SetClearColor(engine->renderer, bg.r, bg.g, bg.b, bg.a);
            Render_Clear(engine->renderer);
        }
        else if (cam_comp->clear_flags == CLEAR_DEPTH_ONLY)
        {
            Render_ClearDepth(engine->renderer);
        }

        // Setup Camera Matrices
        Vector3 global_pos = Transform_GetGlobalPosition(cam_transform);
        packet.view_matrix = Matrix4Inverse(cam_transform->world_matrix);
        Camera_RecalculateProjectionIfNeeded(cam_comp);
        packet.projection_matrix = cam_comp->projection_matrix;
        packet.camera_pos = global_pos;
        packet.has_env_map = (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH) ? scene->has_env_map : false;
        packet.window_width = cam_comp->viewport_width;
        packet.window_height = cam_comp->viewport_height;

        Matrix4 view_proj = Matrix4Multiply(packet.projection_matrix, packet.view_matrix);
        Frustum cam_frustum = Frustum_ExtractFromMatrix(view_proj);

        // Begin Forward Pass
        Render_BeginFrame(engine->renderer, &packet);

        // Submit all visible geometry
        Engine_SubmitVisibleGeometry(engine, scene, probe_capture_requested ? NULL : &cam_frustum, global_pos, cam_comp->culling_masks);

        // End Forward Pass
        Render_EndFrame(engine->renderer);
        Engine_ApplyReflectionProbeResults(engine, scene, active_reflection_probes, packet.reflection_probe_count);
        probe_capture_requested = false;
    }
}