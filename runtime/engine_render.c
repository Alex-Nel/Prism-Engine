#include "engine_runtime.h"
#include <string.h>



#define ENGINE_MAX_GATHER_PROBES 16






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
void Engine_GatherSceneLights(PrismEngine* engine, Scene* scene, RenderLighting* lighting, DirectionalLightData* dir_lights, uint32_t max_dir, PointLightData* point_lights, uint32_t max_point, SpotLightData* spot_lights, uint32_t max_spot)
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

        if (l->type == LIGHT_DIRECTIONAL && dir_count < max_dir)
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
        else if (l->type == LIGHT_POINT && point_count < max_point)
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
        else if (l->type == LIGHT_SPOT && spot_count < max_spot)
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










// Gathers active local IBL probes as value data. Capture results are queried after rendering.
void Engine_GatherReflectionProbes(PrismEngine* engine, Scene* scene, RenderLighting* lighting, ReflectionProbeData* probes, uint32_t max_probes)
{
    uint32_t count = 0;
    const uint32_t required_mask = COMPONENT_TRANSFORM | COMPONENT_REFLECTION_PROBE;

    if (max_probes == 0)
        max_probes = ENGINE_MAX_GATHER_PROBES;
    if (max_probes > ENGINE_MAX_GATHER_PROBES)
        max_probes = ENGINE_MAX_GATHER_PROBES;

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

        if (count < max_probes)
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










// Applies reflection probe capture results from a completed frame back to the components
void Engine_ApplyFrameResults(PrismEngine* engine, Scene* scene, const RenderFrame* frame)
{
    if (!engine || !scene || !frame)
        return;

    for (uint32_t i = 0; i < frame->probe_result_count; i++)
    {
        uint32_t entity_id = frame->probe_results[i].entity_id;
        if (entity_id >= MAX_ENTITIES || !(scene->component_masks[entity_id] & COMPONENT_REFLECTION_PROBE))
            continue;
        
        ReflectionProbeComponent* component = &scene->reflection_probes[entity_id];
        component->environment = frame->probe_results[i].environment;
        component->dirty = frame->probe_results[i].dirty;
        component->captured = frame->probe_results[i].captured;
    }
}










// Applies reflection probe capture results from the last DrawWorld back to the components
void Engine_ApplyReflectionProbeResults(PrismEngine* engine, Scene* scene)
{
    if (!engine || !engine->renderer || !scene)
        return;

    RenderProbeResult results[ENGINE_MAX_GATHER_PROBES];
    uint32_t probe_count = Render_GetProbeResults(engine->renderer, results, ENGINE_MAX_GATHER_PROBES);

    RenderFrame temp = {0};
    temp.probe_result_count = probe_count;

    for (uint32_t i = 0; i < probe_count; i++)
    {
        temp.probe_results[i] = results[i];
    }

    Engine_ApplyFrameResults(engine, scene, &temp);
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





static void RenderFrame_AssignBones(RenderFrame* frame, RenderItem* item, Matrix4* bone_ptr)
{
    if (!frame || !item || !bone_ptr)
        return;

    if (frame->bone_slot_count >= RENDER_FRAME_MAX_SKINNED)
        return;
    
    uint32_t slot = frame->bone_slot_count++;
    memcpy(frame->bone_matrices[slot], bone_ptr, sizeof(Matrix4) * MAX_BONES);
    item->bone_matrices = frame->bone_matrices[slot];
}





static void Extract_TryPushSkinned(RenderItem* out, uint32_t max, uint32_t* count, RenderItem* item, RenderFrame* frame, Matrix4* bone_ptr)
{
    if (frame)
        RenderFrame_AssignBones(frame, item, bone_ptr);
    else
        item->bone_matrices = bone_ptr;

    Extract_TryPush(out, max, count, item);
}










// Extracts scene geometry into a RenderItem array for DrawWorld
uint32_t Engine_GatherVisibleGeometry(Scene* scene, Vector3 cam_pos, uint32_t culling_masks, RenderItem* out, uint32_t max, RenderFrame* frame_for_bones)
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
        
        if (rc->casts_shadows)
            item.flags |= RENDER_ITEM_CAST_SHADOWS;
        if (rc->receives_shadows)
            item.flags |= RENDER_ITEM_RECEIVE_SHADOWS;

        Extract_TryPushSkinned(out, max, &count, &item, frame_for_bones, bone_ptr);
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















// Builds an immutable render snapshot from the current scene state.
void Engine_BuildRenderFrame(PrismEngine* engine, Scene* scene, RenderFrame* frame)
{
    if (!engine || !scene || !frame)
        return;

    RenderFrame_Reset(frame);
    frame->frame_id = ++engine->render_frame_counter;

    frame->width = Platform_GetWindowWidth(engine->window);
    frame->height = Platform_GetWindowHeight(engine->window);

    // Make an empty render packet to send to the renderer
    RendererSettings cur_settings = Render_GetSettings(engine->renderer);
    frame->enable_ssao = cur_settings.enable_ssao;
    frame->global_ambient_color = scene->ambient_color;
    frame->global_ambient_illumination = scene->ambient_illumination;
    frame->exposure = scene->exposure;
    frame->gamma = 2.2f;
    if (cur_settings.gamma > 0.01f)
        frame->gamma = cur_settings.gamma;
    
    frame->has_probe_source_env_map = scene->has_env_map;
    if (scene->has_env_map && scene->env_map)
    {
        frame->env_map = scene->env_map->gpu_handle;
        frame->probe_source_env_map = scene->env_map->gpu_handle;
    }

    // --- Get all Point Lights from the ECS ---
    RenderLighting lighting = {0};
    Engine_GatherSceneLights(engine, scene, &lighting,
        frame->dir_lights, RENDER_FRAME_MAX_DIR_LIGHTS,
        frame->point_lights, RENDER_FRAME_MAX_POINT_LIGHTS,
        frame->spot_lights, RENDER_FRAME_MAX_SPOT_LIGHTS);
    frame->dir_light_count = lighting.dir_light_count;
    frame->point_light_count = lighting.point_light_count;
    frame->spot_light_count = lighting.spot_light_count;


    // Fill the packet with all the lights in the scene
    uint32_t probe_cap = cur_settings.max_reflection_probes;
    if (probe_cap > RENDER_FRAME_MAX_PROBES)
        probe_cap = RENDER_FRAME_MAX_PROBES;
    Engine_GatherReflectionProbes(engine, scene, &lighting, frame->reflection_probes, probe_cap);
    frame->reflection_probe_count = lighting.reflection_probe_count;

    Transform* main_cam_t = &scene->transforms[scene->main_camera_id];
    CameraComponent* main_cam = &scene->cameras[scene->main_camera_id];
    if (frame->width > 0 && frame->height > 0)
    {
        main_cam->viewport_x = 0;
        main_cam->viewport_y = 0;
        main_cam->viewport_width = frame->width;
        main_cam->viewport_height = frame->height;
    }
    Camera_RecalculateProjectionIfNeeded(main_cam);
    frame->shadow_camera_pos = Transform_GetGlobalPosition(main_cam_t);
    frame->camera_forward = Transform_GetForwardVector(main_cam_t);
    frame->camera_right = Transform_GetRightVector(main_cam_t);
    frame->camera_up = Transform_GetUpVector(main_cam_t);
    frame->camera_near = main_cam->nearZ;
    frame->camera_far = main_cam->farZ;
    frame->camera_fov = main_cam->fov;
    frame->camera_aspect = 1.0f;
    if (frame->height > 0)
        frame->camera_aspect = (float)frame->width / (float)frame->height;
    

    // Gather and sort cameras
    ActiveCamera active_cameras[MAX_ENTITIES];
    uint32_t camera_count = Engine_GatherAndSortCameras(engine, scene, active_cameras);
    if (camera_count > RENDER_FRAME_MAX_VIEWS)
        camera_count = RENDER_FRAME_MAX_VIEWS;

    uint32_t max_items = cur_settings.max_draw_items;
    if (max_items == 0 || max_items > RENDER_FRAME_MAX_ITEMS)
        max_items = RENDER_FRAME_MAX_ITEMS;


    for (uint32_t c = 0; c < camera_count; c++)
    {
        uint32_t cam_id = active_cameras[c].entity_id;
        Transform* cam_transform = &scene->transforms[cam_id];
        CameraComponent* cam_comp = &scene->cameras[cam_id];

        Vector3 global_pos = Transform_GetGlobalPosition(cam_transform);
        if (frame->width > 0 && frame->height > 0)
        {
            cam_comp->viewport_x = 0;
            cam_comp->viewport_y = 0;
            cam_comp->viewport_width = frame->width;
            cam_comp->viewport_height = frame->height;
        }
        Camera_RecalculateProjectionIfNeeded(cam_comp);

        RenderFrameView* view_slot = &frame->views[frame->view_count];
        view_slot->item_start = frame->item_count;

        uint32_t viewport_w = cam_comp->viewport_width;
        uint32_t viewport_h = cam_comp->viewport_height;

        view_slot->view.view_matrix = Matrix4Inverse(cam_transform->world_matrix);
        view_slot->view.projection_matrix = cam_comp->projection_matrix;
        view_slot->view.camera_pos = global_pos;
        view_slot->view.has_env_map = (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH) ? scene->has_env_map : false;
        view_slot->view.window_width = viewport_w;
        view_slot->view.window_height = viewport_h;
        view_slot->view.clear_flags = (RenderClearFlags)cam_comp->clear_flags;
        view_slot->view.clear_color = scene->background_color;
        uint32_t remaining = max_items - frame->item_count;
        uint32_t gathered = Engine_GatherVisibleGeometry(scene, global_pos, cam_comp->culling_masks, frame->items + frame->item_count, remaining, frame);
        view_slot->item_count = gathered;
        frame->item_count += gathered;
        frame->view_count++;
    }
}










// Renders a specified scene
void Engine_RenderScene(PrismEngine* engine, Scene* scene)
{
    if (!engine || !scene || !engine->renderer)
        return;

    RenderFrame* frame = &engine->render_frame;
    Engine_BuildRenderFrame(engine, scene, frame);
    
    Render_DrawFrame(engine->renderer, frame);
    frame->probe_result_count = Render_GetProbeResults(engine->renderer, frame->probe_results, RENDER_FRAME_MAX_PROBES);
    Engine_ApplyFrameResults(engine, scene, frame);
}