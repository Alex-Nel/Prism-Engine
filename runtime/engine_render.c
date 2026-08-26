#include "engine_runtime.h"


static RenderItem s_extracted_items[MAX_COMMANDS];



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





// Gathers all the lights in a scene and puts them into a lighting packet.
void Engine_GatherSceneLights(PrismEngine* engine, Scene* scene, RenderLighting* lighting, DirectionalLightData* dir_lights, PointLightData* point_lights, SpotLightData* spot_lights)
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

    lighting->dir_lights = dir_lights;
    lighting->dir_light_count = dir_count;
    lighting->point_lights = point_lights;
    lighting->point_light_count = point_count;
    lighting->spot_lights = spot_lights;
    lighting->spot_light_count = spot_count;
}










// Gathers active local IBL probes as value data. Capture results are copied back explicitly after rendering
void Engine_GatherReflectionProbes(PrismEngine* engine, Scene* scene, RenderLighting* lighting, ReflectionProbeData* probes)
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

    lighting->reflection_probes = probes;
    lighting->reflection_probe_count = count;

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










// Sets material properties for a render item
static void RenderItem_SetMaterial(RenderItem* item, Material* material)
{
    item->material = (MaterialHandle){0};
    item->color = (Color){1.0f, 1.0f, 1.0f, 1.0f};

    if (!material)
        return;
    
    Asset_SyncMaterialGPU(material);

    item->material = material->gpu_handle;
    item->color = material->properties.albedo_tint;
}










static void Extract_TryPush(RenderItem* out, uint32_t max, uint32_t* count, const RenderItem* item)
{
    if (*count >= max)
        return;
    out[(*count)++] = *item;
}










// Extracts scene geometry into a RenderItem array for DrawWorld
uint32_t Engine_GatherVisibleGeometry(Scene* scene, Vector3 cam_pos, uint32_t culling_masks, RenderItem* out, uint32_t max)
{
    uint32_t count = 0;
    if (!scene || !out || max == 0)
        return 0;

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

        RenderItem item = {0};

        item.mesh = rc->mesh->gpu_handle;
        RenderItem_SetMaterial(&item, rc->material);
        item.transform = t->world_matrix;
        item.local_bounds = rc->mesh->local_bounds;
        
        if (rc->casts_shadows)
            item.flags |= RENDER_ITEM_CAST_SHADOWS;
        if (rc->receives_shadows)
            item.flags |= RENDER_ITEM_RECEIVE_SHADOWS;
        if ((scene->component_masks[i] & (COMPONENT_RIGIDBODY | COMPONENT_SCRIPT | COMPONENT_ANIMATOR)) == 0)
            item.flags |= RENDER_ITEM_PROBE_CAPTURE;

        Extract_TryPush(out, max, &count, &item);
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

        Matrix4* bone_ptr = NULL;
        uint32_t anim_id = rc->root_animator_entity_id;

        if (anim_id != 0 && anim_id != ENTITY_NONE && (scene->component_masks[anim_id] & COMPONENT_ANIMATOR))
            bone_ptr = scene->animators[anim_id].final_bone_matrices;
        else if (scene->component_masks[i] & COMPONENT_ANIMATOR)
            bone_ptr = scene->animators[i].final_bone_matrices;

        RenderItem item = {0};

        item.mesh = rc->mesh->gpu_handle;
        RenderItem_SetMaterial(&item, rc->material);
        item.transform = t->world_matrix;
        item.local_bounds = rc->pose_bounds;
        item.bone_matrices = bone_ptr;
        
        if (rc->casts_shadows)
            item.flags |= RENDER_ITEM_CAST_SHADOWS;
        if (rc->receives_shadows)
            item.flags |= RENDER_ITEM_RECEIVE_SHADOWS;

        Extract_TryPush(out, max, &count, &item);
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

        RenderItem item = {0};

        item.mesh = line->dynamic_mesh->gpu_handle;
        RenderItem_SetMaterial(&item, line->material);
        item.color = line->color;
        item.transform = Matrix4Identity();
        item.local_bounds = line->dynamic_mesh->local_bounds;

        Extract_TryPush(out, max, &count, &item);
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

        float aspect_x = (float)sprite->material->albedo_texture->width / (float)sprite->material->albedo_texture->height;
        Matrix4 aspect_matrix = Matrix4Identity();
        aspect_matrix.m00 = aspect_x;
        
        Matrix4 final_sprite_matrix = Matrix4Multiply(t->world_matrix, aspect_matrix);

        RenderItem item = {0};

        item.mesh = sprite->quad->gpu_handle;
        RenderItem_SetMaterial(&item, sprite->material);
        item.color = sprite->color;
        item.transform = final_sprite_matrix;
        item.local_bounds = sprite->quad->local_bounds;
        item.depth_distance = dist_sq;
        item.flags = RENDER_ITEM_TRANSPARENT;

        Extract_TryPush(out, max, &count, &item);
    }

    return count;
}










// Submits all scene geometry via the streaming Begin/Submit/End path
void Engine_SubmitVisibleGeometry(PrismEngine* engine, Scene* scene, Vector3 cam_pos, uint32_t culling_masks)
{
    if (!engine || !engine->renderer)
        return;

    uint32_t count = Engine_GatherVisibleGeometry(scene, cam_pos, culling_masks, s_extracted_items, MAX_COMMANDS);
    
    for (uint32_t i = 0; i < count; i++)
        Render_Submit(engine->renderer, &s_extracted_items[i]);
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
    RenderLighting lighting = {0};
    RendererSettings cur_settings = Render_GetSettings(engine->renderer);
    lighting.enable_ssao = cur_settings.enable_ssao;
    lighting.global_ambient_color = scene->ambient_color;
    lighting.global_ambient_illumination = scene->ambient_illumination;
    lighting.exposure = scene->exposure;
    lighting.gamma = 2.2f;
    if (cur_settings.gamma > 0.01f)
        lighting.gamma = cur_settings.gamma;
    
    lighting.has_probe_source_env_map = scene->has_env_map;

    if (scene->has_env_map && scene->env_map)
    {
        lighting.env_map = *scene->env_map;
        lighting.probe_source_env_map = *scene->env_map;
    }

    // --- Get all Point Lights from the ECS ---
    DirectionalLightData active_dir_lights[MAX_RESOURCES];
    PointLightData active_point_lights[MAX_RESOURCES];
    SpotLightData active_spot_lights[MAX_RESOURCES];
    ReflectionProbeData active_reflection_probes[MAX_REFLECTION_PROBES];


    // Fill the packet with all the lights in the scene
    Engine_GatherSceneLights(engine, scene, &lighting, active_dir_lights, active_point_lights, active_spot_lights);
    Engine_GatherReflectionProbes(engine, scene, &lighting, active_reflection_probes);

    Transform* main_cam_t = &scene->transforms[scene->main_camera_id];
    CameraComponent* main_cam = &scene->cameras[scene->main_camera_id];
    Camera_RecalculateProjectionIfNeeded(main_cam);
    lighting.shadow_camera_pos = Transform_GetGlobalPosition(main_cam_t);
    lighting.camera_forward = Transform_GetForwardVector(main_cam_t);
    lighting.camera_right = Transform_GetRightVector(main_cam_t);
    lighting.camera_up = Transform_GetUpVector(main_cam_t);
    lighting.camera_near = main_cam->nearZ;
    lighting.camera_far = main_cam->farZ;
    lighting.camera_fov = main_cam->fov;
    lighting.camera_aspect = 1.0f;
    if (main_cam->viewport_height > 0)
        lighting.camera_aspect = (float)main_cam->viewport_width / (float)main_cam->viewport_height;
    

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

        // Setup Camera Matrices and clearing flags
        Vector3 global_pos = Transform_GetGlobalPosition(cam_transform);
        Camera_RecalculateProjectionIfNeeded(cam_comp);

        RenderView view = {0};
        view.view_matrix = Matrix4Inverse(cam_transform->world_matrix);
        view.projection_matrix = cam_comp->projection_matrix;
        view.camera_pos = global_pos;
        view.has_env_map = (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH) ? scene->has_env_map : false;
        view.window_width = cam_comp->viewport_width;
        view.window_height = cam_comp->viewport_height;
        view.clear_flags = (RenderClearFlags)cam_comp->clear_flags;
        view.clear_color = scene->background_color;

        uint32_t item_count = Engine_GatherVisibleGeometry(scene, global_pos, cam_comp->culling_masks, s_extracted_items, MAX_COMMANDS);
        RenderWorld world = {
            .view = view,
            .lighting = lighting,
            .items = s_extracted_items,
            .item_count = item_count
        };
        Render_DrawWorld(engine->renderer, &world);

        Engine_ApplyReflectionProbeResults(engine, scene, active_reflection_probes, lighting.reflection_probe_count);
    }
}