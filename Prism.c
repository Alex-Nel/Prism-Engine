#include "Prism.h"
#include <stddef.h>
#include <stdlib.h>


// Internal state
static PrismEngine engine;

static EngineUpdateCallback g_PreUpdateCallback = NULL;


// Initializes all engine systems
bool Engine_Init(const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine.target_fps = target_fps;

    // Platform Init
    engine.window = Platform_Init(window_title, window_width, window_height, api);
    if (!engine.window && api != GRAPHICS_API_NONE)
    {
        Log_Error("Window failed to initialize.\n");
        return false;
    }
    Log_Info("Window Initialized");

    // Get the Procedure address if OpenGL is used
    void* proc_addr;
    if (api == GRAPHICS_API_OPENGL)
        proc_addr = Platform_GetProcAddress;
    else
        proc_addr = NULL;
    
    // Render Init
    Renderer* renderer = Render_Init(api, proc_addr);
    if (!renderer)
    {
        Platform_Shutdown(engine.window);
        Log_Error("Renderer failed to initialize.\n");
        return false;
    }
    engine.renderer = renderer;
    Log_Info("Renderer Initialized");

    // Set renderer clear color to pure white
    Engine_SetClearColor(0.8f, 0.8f, 0.8f, 1.0f);

    engine.is_running = true;

    // Core moduels init
    Input_Init();
    Audio_Init();
    Asset_Init(renderer);
    Time_Init(engine.target_fps, Platform_GetTime, Platform_Delay);

    return true;
}





// Get a pointer to the main window
Window* Engine_GetMainWindow()
{
    return engine.window;
}





// Sets the custom callback function
void Engine_SetPreUpdateCallback(EngineUpdateCallback callback)
{
    g_PreUpdateCallback = callback;
}





// A function to process window events without pausing main loop
static void Engine_OnModalEvent(void* userdata)
{
    Scene* active_scene = (Scene*)userdata;
    if (!active_scene) return;

    // Force the renderer to update its viewport immediately
    uint32_t w = Platform_GetWindowWidth(engine.window);
    uint32_t h = Platform_GetWindowHeight(engine.window);
    
    if (w > 0 && h > 0)
        Render_SetViewport(engine.renderer, 0, 0, w, h);

    // Tick the time to prevent physics/animation explosions when we let go
    Time_Tick();

    // Update scripts/animations
    Scene_Update(active_scene);

    // Render and Swap Buffers directly!
    Engine_RenderScene(active_scene);
    Platform_SwapBuffers(engine.window);
}





// Runs the engine, updates and renders the scene
void Engine_Run(Scene* active_scene)
{
    if (!active_scene)
    {
        Log_Error("Cannot run engine without an active scene");
        return;
    }

    Platform_SetEventWatchCallback(Engine_OnModalEvent, active_scene);

    Log_Info("Running Scene");

    float accumulator = 0.0f;

    while (engine.is_running)
    {
        // Advance the engine clock
        Time_Tick();

        accumulator += Time_DeltaTime();
        
        // Poll through events
        Event e;
        while (Platform_PollEvents(&e))
        {
            Input_ProcessEvent(&e);
            
            if (e.type == EVENT_WINDOW_CLOSE)
            {
                engine.is_running = false;
            }
            else if (e.type == EVENT_WINDOW_RESIZE)
            {
                Render_SetViewport(engine.renderer, 0, 0, e.window_resize.width, e.window_resize.height);
                Platform_SetWindowSize(engine.window, e.window_resize.width, e.window_resize.height);
            }
        }

        // If the API registered a custom callback, call it
        if (g_PreUpdateCallback != NULL)
            g_PreUpdateCallback();

        // Update accumulator and fixed updates
        float fixed_dt = Time_FixedDeltaTime();
        while (accumulator >= fixed_dt)
        {
            Scene_FixedUpdate(active_scene);
            accumulator -= fixed_dt;
        }

        // Update scene and physics
        Scene_Update(active_scene);

        // Render scene
        Engine_RenderScene(active_scene);

        // Process destroy queue
        Scene_ProcessDestroyQueue(active_scene);
        
        // Swap Buffers & Reset Input arrays
        Engine_EndFrame(); 
    }
}





// *Deprecated* - Use Engine_Run instead
// Continues running the engine. Returns true if it's still running
bool Engine_IsRunning()
{
    if (!engine.is_running) return false;

    // Advance the engine clock
    Time_Tick();

    // Event Routing Loop
    Event e;
    while (Platform_PollEvents(&e))
    {
        // Feed the input manager
        Input_ProcessEvent(&e);

        // Route structural events to other modules
        switch (e.type)
        {
            case EVENT_WINDOW_CLOSE:
                engine.is_running = false;
                break;
                
            case EVENT_WINDOW_RESIZE:
                Render_SetViewport(engine.renderer, 0, 0, e.window_resize.width, e.window_resize.height);
                Platform_SetWindowSize(engine.window, e.window_resize.width, e.window_resize.height);
                Log_Info("Window resized to: %d, %d\n", e.window_resize.width, e.window_resize.height);
                break;
                
            default:
                break;
        }
    }

    return engine.is_running;
}





// Sets the clear color of the renderer
void Engine_SetClearColor(float red, float green, float blue, float alpha)
{
    Render_SetClearColor(engine.renderer, red, green, blue, alpha);
}





// Small struct to sort cameras before rendering
typedef struct ActiveCamera
{
    uint32_t entity_id;
    int render_order;
} ActiveCamera;

// Sorting function for cameras (lowest order renders first)
int CompareCameraOrder(const void* a, const void* b)
{
    return ((ActiveCamera*)a)->render_order - ((ActiveCamera*)b)->render_order;
}





// Renders a specified scene
void Engine_RenderScene(Scene* scene)
{
    if (!scene)
        return;

    CameraComponent* cam = &scene->cameras[scene->main_camera_id];
    cam->viewport_x = 0;
    cam->viewport_y = 0;
    cam->viewport_width = (uint32_t)Platform_GetWindowWidth(engine.window);
    cam->viewport_height = (uint32_t)Platform_GetWindowHeight(engine.window);

    // Make an empty render packet to send to the renderer
    RenderPacket packet = {0};

    // --- Get all Point Lights from the ECS ---
    DirectionalLightData active_dir_lights[MAX_RESOURCES];
    PointLightData active_point_lights[MAX_RESOURCES];
    SpotLightData active_spot_lights[MAX_RESOURCES];

    uint32_t directional_count = 0;
    uint32_t point_count = 0;
    uint32_t spot_count = 0;

    uint32_t light_mask = COMPONENT_TRANSFORM | COMPONENT_LIGHT;

    float pi = 3.14159265359f;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i])
            continue;

        if ((scene->component_masks[i] & light_mask) == light_mask)
        {    
            Transform* t = &scene->transforms[i];
            LightComponent* l = &scene->lights[i];

            if (!l->is_active)
                continue;

            if (l->type == LIGHT_DIRECTIONAL && directional_count < MAX_RESOURCES)
            {
                active_dir_lights[directional_count].direction = Transform_GetForwardVector(t);
                active_dir_lights[directional_count].color = l->color;
                active_dir_lights[directional_count].intensity = l->intensity;
                active_dir_lights[directional_count].ambient_strength = l->ambient_strength;
                active_dir_lights[directional_count].shadow_box_size = l->shadow_box_size;
                active_dir_lights[directional_count].shadow_cascade_count = l->shadow_cascade_count;
                active_dir_lights[directional_count].shadow_max_distance = l->shadow_max_distance;
                active_dir_lights[directional_count].cascade_split_lambda = l->cascade_split_lambda;
                active_dir_lights[directional_count].cascade_blend_fraction = l->cascade_blend_fraction;
                directional_count++;
            }
            else if (l->type == LIGHT_POINT && point_count < MAX_RESOURCES)
            {
                active_point_lights[point_count].position = Transform_GetGlobalPosition(t);
                active_point_lights[point_count].color = l->color;
                active_point_lights[point_count].intensity = l->intensity;
                active_point_lights[point_count].constant = l->constant;
                active_point_lights[point_count].linear = l->linear;
                active_point_lights[point_count].quadratic = l->quadratic;
                point_count++;
            }
            else if (l->type == LIGHT_SPOT && spot_count < MAX_RESOURCES)
            {
                active_spot_lights[spot_count].position = Transform_GetGlobalPosition(t);
                active_spot_lights[spot_count].direction = Transform_GetForwardVector(t);
                active_spot_lights[spot_count].color = l->color;
                active_spot_lights[spot_count].intensity = l->intensity;
                active_spot_lights[spot_count].constant = l->constant;
                active_spot_lights[spot_count].linear = l->linear;
                active_spot_lights[spot_count].quadratic = l->quadratic;
                active_spot_lights[spot_count].inner_cut_off = cosf(l->inner_cut_off * (pi / 180.0f));
                active_spot_lights[spot_count].outer_cut_off = cosf(l->outer_cut_off * (pi / 180.0f));
                spot_count++;
            }
        }
    }

    packet.dir_lights = active_dir_lights;
    packet.dir_light_count = directional_count;

    packet.point_lights = active_point_lights;
    packet.point_light_count = point_count;

    packet.spot_lights = active_spot_lights;
    packet.spot_light_count = spot_count;

    packet.has_skybox = scene->has_skybox;
    packet.skybox_texture = scene->skybox.texture->gpu_handle;
    packet.skybox_shader = scene->skybox.shader->gpu_handle;



    // --- Shadow Pass ---

    if (directional_count > 0)
    {
        DirectionalLightData* shadow_light = &active_dir_lights[0];
        Vector3 light_dir = shadow_light->direction;

        Transform* main_cam_t = &scene->transforms[scene->main_camera_id];
        CameraComponent* main_cam = &scene->cameras[scene->main_camera_id];
        Vector3 cam_pos = Transform_GetGlobalPosition(main_cam_t);
        Vector3 cam_fwd = Transform_GetForwardVector(main_cam_t);
        Vector3 cam_right = Transform_GetRightVector(main_cam_t);
        Vector3 cam_up = Transform_GetUpVector(main_cam_t);

        packet.camera_forward = cam_fwd;

        Camera_RecalculateProjectionIfNeeded(main_cam);
        packet.shadow_camera_near = main_cam->nearZ;
        packet.cascade_blend_fraction = shadow_light->cascade_blend_fraction;
        if (packet.cascade_blend_fraction <= 0.0f)
            packet.cascade_blend_fraction = 0.12f;
        if (packet.cascade_blend_fraction > 0.5f)
            packet.cascade_blend_fraction = 0.5f;

        float light_distance = 80.0f;
        Vector3 up = (fabsf(light_dir.y) > 0.99f) ? (Vector3){0.0f, 0.0f, 1.0f} : (Vector3){0.0f, 1.0f, 0.0f};

        uint32_t cascade_count = shadow_light->shadow_cascade_count;
        if (cascade_count < 1)
            cascade_count = 1;
        if (cascade_count > MAX_SHADOW_CASCADES)
            cascade_count = MAX_SHADOW_CASCADES;

        packet.shadow_cascade_count = cascade_count;

        if (cascade_count <= 1)
        {
            // --- Single shadow map (original camera-following ortho box) ---
            float shadow_box_size = shadow_light->shadow_box_size;
            if (shadow_box_size <= 0.0f)
                shadow_box_size = 20.0f;
            
            Vector3 center = {
                cam_pos.x + cam_fwd.x * shadow_box_size * 0.5f,
                cam_pos.y + cam_fwd.y * shadow_box_size * 0.5f,
                cam_pos.z + cam_fwd.z * shadow_box_size * 0.5f
            };

            float texel_world_size = (2.0f * shadow_box_size) / (float)SHADOW_MAP_RESOLUTION;

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

            packet.light_space_matrices[0] = Matrix4Multiply(light_proj, light_view);
            packet.shadow_texel_world_sizes[0] = texel_world_size;
        }
        else
        {
            // --- Cascaded shadow maps ---
            Camera_RecalculateProjectionIfNeeded(main_cam);
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
                packet.cascade_splits[i - 1] = splits[i - 1];
            }

            // cam->fov is stored in radians
            float tan_half = tanf(main_cam->fov * 0.5f);

            for (uint32_t c = 0; c < cascade_count; c++)
            {
                float split_near = (c == 0) ? cam_near : splits[c - 1];
                float split_far  = (c == cascade_count - 1) ? shadow_far : splits[c];

                // Extend the rendered frustum slightly past the split so the next cascade has overlapping shadow data for a smooth cross-fade in the shader.
                if (c > 0)
                {
                    float prev_near = (c == 1) ? cam_near : splits[c - 2];
                    float prev_slice = splits[c - 1] - prev_near;
                    split_near -= prev_slice * packet.cascade_blend_fraction;
                    if (split_near < cam_near)
                        split_near = cam_near;
                }

                Vector3 corners[8];
                BuildFrustumSliceCorners(cam_pos, cam_fwd, cam_right, cam_up, aspect, tan_half, split_near, split_far, corners);

                ComputeCascadeLightMatrix(corners, light_dir, up, light_distance, &packet.light_space_matrices[c], &packet.shadow_texel_world_sizes[c]);
            }
        }


        // Begin Shadow Pass
        Render_BeginShadowPass(engine.renderer, &packet);

        // Get the shadow shaders (static + skinned variants)
        ShaderHandle shadow_shader = Asset_GetDefaultShadowShader()->gpu_handle;
        ShaderHandle skinned_shadow_shader = Asset_GetDefaultSkinnedShadowShader()->gpu_handle;

        // Submit Static Meshes for Shadows
        uint32_t req_mesh_mask = COMPONENT_TRANSFORM | COMPONENT_MESH_RENDERER;
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;
            
            if ((scene->component_masks[i] & req_mesh_mask) == req_mesh_mask)
            {
                MeshRendererComponent* rc = &scene->mesh_renderers[i];
                if (!rc->is_active || !rc->mesh)
                    continue;

                Transform* t = &scene->transforms[i];
                // Pass NULL for texture and default props, the shadow shader ignores them!
                Render_Submit(engine.renderer, rc->mesh->gpu_handle, shadow_shader,
                              (TextureHandle){0}, (MaterialProperties){0},
                              t->world_matrix, NULL, false, 0.0f);
            }
        }

        // Submit Skinned Meshes for Shadows
        uint32_t req_skin_mask = COMPONENT_TRANSFORM | COMPONENT_SKINNED_MESH_RENDERER;
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;

            if ((scene->component_masks[i] & req_skin_mask) == req_skin_mask)
            {
                SkinnedMeshRendererComponent* rc = &scene->skinned_mesh_renderers[i];
                if (!rc->is_active || !rc->mesh)
                    continue;

                Transform* t = &scene->transforms[i];
                Matrix4* bone_ptr = NULL;
                uint32_t anim_id = rc->root_animator_entity_id;
                
                if (anim_id != 0 && anim_id != ENTITY_NONE && (scene->component_masks[anim_id] & COMPONENT_ANIMATOR))
                    bone_ptr = scene->animators[anim_id].final_bone_matrices;
                else if (scene->component_masks[i] & COMPONENT_ANIMATOR)
                    bone_ptr = scene->animators[i].final_bone_matrices;

                // Use the skinned shadow shader so the cast shadow follows the live pose.
                // If there are no bones, fall back to the static shader (bind pose).
                ShaderHandle caster_shader = (bone_ptr != NULL) ? skinned_shadow_shader : shadow_shader;

                Render_Submit(engine.renderer, rc->mesh->gpu_handle, caster_shader,
                              (TextureHandle){0}, (MaterialProperties){0},
                              t->world_matrix, bone_ptr, false, 0.0f);
            }
        }

        Render_EndShadowPass(engine.renderer);
    }



    // --- Gather and sort camera ---

    ActiveCamera active_cameras[MAX_ENTITIES];
    uint32_t camera_count = 0;
    uint32_t cam_mask = COMPONENT_TRANSFORM | COMPONENT_CAMERA;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i])
            continue;
        
        if ((scene->component_masks[i] & cam_mask) == cam_mask)
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
    


    // --- Execute render pass per camera ---

    for (uint32_t c = 0; c < camera_count; c++)
    {
        uint32_t cam_id = active_cameras[c].entity_id;
        Transform* cam_transform = &scene->transforms[cam_id];
        CameraComponent* cam_comp = &scene->cameras[cam_id];

        // Clear the screen based on this camera's flags
        if (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH)
        {
            // Sets background color and wipes depth
            Color bg = scene->skybox.background_color;
            Render_SetClearColor(engine.renderer, bg.r, bg.g, bg.b, bg.a);
            Render_Clear(engine.renderer);
        }
        else if (cam_comp->clear_flags == CLEAR_DEPTH_ONLY)
        {
            Render_ClearDepth(engine.renderer);
        }

        // Build Camera Matrices
        Vector3 global_pos = Transform_GetGlobalPosition(cam_transform);
        packet.view_matrix = Matrix4Inverse(cam_transform->world_matrix);

        // Force camera to sync
        Camera_RecalculateProjectionIfNeeded(cam_comp);
        packet.projection_matrix = cam_comp->projection_matrix;
        packet.camera_pos = global_pos;

        if (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH)
            packet.has_skybox = scene->has_skybox;
        else
            packet.has_skybox = false;


        // Calculate the frustum for this camera
        Matrix4 view_proj = Matrix4Multiply(packet.projection_matrix, packet.view_matrix);
        Frustum cam_frustum = Frustum_ExtractFromMatrix(view_proj);

        packet.window_width = cam->viewport_width;
        packet.window_height = cam->viewport_height;

        // Start pass
        Render_BeginFrame(engine.renderer, &packet);



        // --- Submit static meshes for this camera only ---

        uint32_t required_mesh_mask = COMPONENT_TRANSFORM | COMPONENT_MESH_RENDERER;
        
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;

            if ((scene->component_masks[i] & required_mesh_mask) == required_mesh_mask)
            {
                MeshRendererComponent* rc = &scene->mesh_renderers[i];

                if (!rc->is_active || !rc->mesh || !rc->material)
                    continue;

                if ((rc->layer_mask & cam_comp->culling_masks) == 0)
                    continue;
                
                Transform* t = &scene->transforms[i];

                // Skip if its outside the frustum
                if (!Frustum_ContainsAABB(&cam_frustum, rc->mesh->local_bounds, t->world_matrix))
                    continue;

                Render_Submit(engine.renderer, rc->mesh->gpu_handle, rc->material->shader->gpu_handle,
                                rc->material->diffuse_texture->gpu_handle, rc->material->properties,
                                t->world_matrix, NULL, false, 0.0f);
            }
        }



        // --- Submit skinned meshes for this camera only ---

        uint32_t required_skinned_mask = COMPONENT_TRANSFORM | COMPONENT_SKINNED_MESH_RENDERER;

        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;

            if ((scene->component_masks[i] & required_skinned_mask) == required_skinned_mask)
            {
                SkinnedMeshRendererComponent* rc = &scene->skinned_mesh_renderers[i];

                if (!rc->is_active || !rc->mesh || !rc->material)
                    continue;

                if ((rc->layer_mask & cam_comp->culling_masks) == 0)
                    continue;

                Transform* t = &scene->transforms[i];

                Matrix4* bone_ptr = NULL;

                // Grab the explicit Animator Entity that drives this mesh
                uint32_t anim_id = rc->root_animator_entity_id;
                
                // If it points to a parent/root entity, get its bones
                if (anim_id != 0 && anim_id != ENTITY_NONE)
                {
                    if (scene->component_masks[anim_id] & COMPONENT_ANIMATOR)
                        bone_ptr = scene->animators[anim_id].final_bone_matrices;
                }
                // Fallback: If the Animator is on this exact same entity
                else if (scene->component_masks[i] & COMPONENT_ANIMATOR)
                {
                    bone_ptr = scene->animators[i].final_bone_matrices;
                }

                // Use cached pose bounds for frustum culling
                if (!Frustum_ContainsAABB(&cam_frustum, rc->pose_bounds, t->world_matrix))
                    continue;

                Render_Submit(engine.renderer, rc->mesh->gpu_handle, rc->material->shader->gpu_handle,
                              rc->material->diffuse_texture->gpu_handle, rc->material->properties,
                              t->world_matrix, bone_ptr, false, 0.0f);
            }
        }



        // --- Submit Line Renderers ---

        uint32_t required_line_mask = COMPONENT_TRANSFORM | COMPONENT_LINE_RENDERER;

        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;

            if ((scene->component_masks[i] & required_line_mask) == required_line_mask)
            {
                LineRendererComponent* line = &scene->line_renderers[i];

                if (!line->is_active || line->point_count < 2)
                    continue;

                // Ensure the dynamic mesh and material exist
                if (line->dynamic_mesh && line->material)
                {
                    // TODO: Add a layer_mask to LineRendererComponents
                    // if ((line->layer_mask & cam_comp->culling_masks) == 0) continue;

                    MaterialProperties local_props = line->material->properties;
    
                    // 2. Override the tint on the copy ONLY
                    local_props.tint_color = line->color;

                    Render_Submit(engine.renderer, line->dynamic_mesh->gpu_handle, 
                                  line->material->shader->gpu_handle,
                                  line->material->diffuse_texture->gpu_handle, 
                                  local_props,
                                  Matrix4Identity(), NULL, false, 0.0f);
                }
            }
        }



        // --- Submit Sprite Renderers ---
        uint32_t required_sprite_mask = COMPONENT_TRANSFORM | COMPONENT_SPRITE_RENDERER;

        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;

            if ((scene->component_masks[i] & required_sprite_mask) == required_sprite_mask)
            {
                SpriteRendererComponent* sprite = &scene->sprite_renderers[i];
                if (!sprite->is_active || !sprite->quad || !sprite->material)
                    continue;

                Transform* t = &scene->transforms[i];

                // Calculate squared distance from the camera for Alpha Sorting!
                Vector3 sprite_pos = Transform_GetGlobalPosition(t);
                float dx = global_pos.x - sprite_pos.x; // global_pos is the camera position from earlier in the function
                float dy = global_pos.y - sprite_pos.y;
                float dz = global_pos.z - sprite_pos.z;
                float dist_sq = (dx * dx) + (dy * dy) + (dz * dz);

                MaterialProperties local_props = sprite->material->properties;
                local_props.tint_color = sprite->color;


                // Get the width/height of the texture
                float tex_w = (float)sprite->material->diffuse_texture->width;
                float tex_h = (float)sprite->material->diffuse_texture->height;
                
                // Calculate aspect ratio. e.g., A tall texture might be 0.5 ratio.
                float aspect_x = tex_w / tex_h;
                float aspect_y = 1.0f; // Keep Y standard, scale X to match

                // Create a temporary matrix that scales the 1x1 quad to the correct aspect ratio
                Matrix4 aspect_matrix = Matrix4Identity();
                aspect_matrix.m00 = aspect_x;
                aspect_matrix.m11 = aspect_y;
                
                // Multiply the Entity's World Matrix by the Aspect Matrix
                Matrix4 final_sprite_matrix = Matrix4Multiply(t->world_matrix, aspect_matrix);


                Render_Submit(engine.renderer, sprite->quad->gpu_handle,
                              sprite->material->shader->gpu_handle,
                              sprite->material->diffuse_texture->gpu_handle,
                              local_props, final_sprite_matrix, NULL, true, dist_sq);
            }
        }


        // End pass
        Render_EndFrame(engine.renderer);
    }
}





// Renders the frame, swaps the buffers and updates the input
void Engine_EndFrame()
{
    if (!engine.is_running)
        return;

    // Swap the OS window buffers to display the new frame
    Platform_SwapBuffers(engine.window);

    // Cycle the input arrays for the next frame
    Input_Update();
}





// Shuts down the renderer and platform
void Engine_Shutdown()
{
    Audio_Shutdown();
    Render_Shutdown(engine.renderer);
    if (engine.window)
    {
        Platform_Shutdown(engine.window);
    }
}










// Captures the mouse to the window
void Engine_CaptureMouse()
{
    Platform_SetRelativeMouseMode(engine.window, true);
}





// Releases the mouse from the window
void Engine_ReleaseMouse()
{
    Platform_SetRelativeMouseMode(engine.window, false);
    Platform_WarpMouseToMiddle(engine.window);
}





// Returns if the mouse is captured
bool Engine_IsMouseCaptured()
{
    return Platform_IsMouseCaptured(engine.window);
}









// Sets the engines target FPS
void Engine_SetTargetFPS(uint32_t fps)
{
    engine.target_fps = fps;
    Time_SetTargetFPS(fps);
}


// Returns the current target FPS
uint32_t Engine_GetTargetFPS()
{
    return Time_GetTargetFPS();
}















// --- Frustum Function ---



// Normalizes the plane equation so the normal has a length of 1
static inline void NormalizePlane(FrustumPlane* p) 
{
    float length = sqrtf(p->normal.x * p->normal.x + p->normal.y * p->normal.y + p->normal.z * p->normal.z);
    
    if (length > 0.0001f)
    {
        p->normal.x /= length;
        p->normal.y /= length;
        p->normal.z /= length;
        p->distance /= length;
    }
}





// Extracts the 6 planes from a view-projection matrix
Frustum Frustum_ExtractFromMatrix(Matrix4 vp)
{
    Frustum f;

    // Left Plane (Row 3 + Row 0)
    f.planes[0].normal.x = vp.m30 + vp.m00;
    f.planes[0].normal.y = vp.m31 + vp.m01;
    f.planes[0].normal.z = vp.m32 + vp.m02;
    f.planes[0].distance = vp.m33 + vp.m03;

    // Right Plane (Column 3 - Row 0)
    f.planes[1].normal.x = vp.m30 - vp.m00;
    f.planes[1].normal.y = vp.m31 - vp.m01;
    f.planes[1].normal.z = vp.m32 - vp.m02;
    f.planes[1].distance = vp.m33 - vp.m03;

    // Bottom Plane (Row 3 + Row 1)
    f.planes[2].normal.x = vp.m30 + vp.m10;
    f.planes[2].normal.y = vp.m31 + vp.m11;
    f.planes[2].normal.z = vp.m32 + vp.m12;
    f.planes[2].distance = vp.m33 + vp.m13;

    // Top Plane (Row 3 - Row 1)
    f.planes[3].normal.x = vp.m30 - vp.m10;
    f.planes[3].normal.y = vp.m31 - vp.m11;
    f.planes[3].normal.z = vp.m32 - vp.m12;
    f.planes[3].distance = vp.m33 - vp.m13;

    // Near Plane (Row 3 + Row 2)
    f.planes[4].normal.x = vp.m30 + vp.m20;
    f.planes[4].normal.y = vp.m31 + vp.m21;
    f.planes[4].normal.z = vp.m32 + vp.m22;
    f.planes[4].distance = vp.m33 + vp.m23;

    // Far Plane (Row 3 - Row 2)
    f.planes[5].normal.x = vp.m30 - vp.m20;
    f.planes[5].normal.y = vp.m31 - vp.m21;
    f.planes[5].normal.z = vp.m32 - vp.m22;
    f.planes[5].distance = vp.m33 - vp.m23;

    // Normalize all planes
    for (int i = 0; i < 6; i++)
        NormalizePlane(&f.planes[i]);

    return f;
}





// Checks if a sphere is inside the frustum
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix)
{
    // Calculate local center and extents (half-sizes)
    Vector3 center_local = {
        (local_aabb.max.x + local_aabb.min.x) * 0.5f,
        (local_aabb.max.y + local_aabb.min.y) * 0.5f,
        (local_aabb.max.z + local_aabb.min.z) * 0.5f
    };

    Vector3 extents_local = {
        (local_aabb.max.x - local_aabb.min.x) * 0.5f,
        (local_aabb.max.y - local_aabb.min.y) * 0.5f,
        (local_aabb.max.z - local_aabb.min.z) * 0.5f
    };

    // Transform the local center into a global center
    Vector3 center_world;
    center_world.x = world_matrix.m00 * center_local.x + world_matrix.m01 * center_local.y + world_matrix.m02 * center_local.z + world_matrix.m03;
    center_world.y = world_matrix.m10 * center_local.x + world_matrix.m11 * center_local.y + world_matrix.m12 * center_local.z + world_matrix.m13;
    center_world.z = world_matrix.m20 * center_local.x + world_matrix.m21 * center_local.y + world_matrix.m22 * center_local.z + world_matrix.m23;

    // Transform the local extents into global extents (Uses absolute rotation/scale values)
    Vector3 extents_world;
    extents_world.x = fabsf(world_matrix.m00) * extents_local.x + fabsf(world_matrix.m01) * extents_local.y + fabsf(world_matrix.m02) * extents_local.z;
    extents_world.y = fabsf(world_matrix.m10) * extents_local.x + fabsf(world_matrix.m11) * extents_local.y + fabsf(world_matrix.m12) * extents_local.z;
    extents_world.z = fabsf(world_matrix.m20) * extents_local.x + fabsf(world_matrix.m21) * extents_local.y + fabsf(world_matrix.m22) * extents_local.z;

    // Test the generated World AABB against the 6 planes
    for (int i = 0; i < 6; i++) 
    {
        FrustumPlane plane = frustum->planes[i];

        // Compute the "radius" of the AABB along this specific plane's normal
        float r = extents_world.x * fabsf(plane.normal.x) +
                  extents_world.y * fabsf(plane.normal.y) +
                  extents_world.z * fabsf(plane.normal.z);

        // Compute distance from the center of the AABB to the plane
        float d = plane.normal.x * center_world.x +
                  plane.normal.y * center_world.y +
                  plane.normal.z * center_world.z +
                  plane.distance;

        // If the distance is less than -r, the box is completely behind the plane
        if (d < -r)
            return false;
    }

    return true; // The box is visible
}





// Builds a texel-snapped light-space matrix that fully contains the eight frustum corners of one cascade slice
void ComputeCascadeLightMatrix(const Vector3 corners[8], Vector3 light_dir, Vector3 up, float light_distance, Matrix4* out_light_space, float* out_texel_world_size)
{
    Vector3 center = {0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 8; k++)
    {
        center.x += corners[k].x;
        center.y += corners[k].y;
        center.z += corners[k].z;
    }
    center.x *= 0.125f;
    center.y *= 0.125f;
    center.z *= 0.125f;

    Vector3 light_pos = {
        center.x - light_dir.x * light_distance,
        center.y - light_dir.y * light_distance,
        center.z - light_dir.z * light_distance
    };
    Matrix4 light_view = Matrix4LookAt(light_pos, center, up);

    float min_x = FLT_MAX, max_x = -FLT_MAX;
    float min_y = FLT_MAX, max_y = -FLT_MAX;

    for (int k = 0; k < 8; k++)
    {
        Vector4 ls = Matrix4MultiplyVector4(light_view, (Vector4){corners[k].x, corners[k].y, corners[k].z, 1.0f});
        if (ls.x < min_x) min_x = ls.x;
        if (ls.x > max_x) max_x = ls.x;
        if (ls.y < min_y) min_y = ls.y;
        if (ls.y > max_y) max_y = ls.y;
    }

    float texel_world_size = fmaxf(max_x - min_x, max_y - min_y) / (float)SHADOW_MAP_RESOLUTION;
    if (texel_world_size <= 0.0f)
        texel_world_size = 0.001f;

    // Snap XY bounds outward to whole texels so shadows stay stable when the camera moves.
    min_x = floorf(min_x / texel_world_size) * texel_world_size;
    min_y = floorf(min_y / texel_world_size) * texel_world_size;
    max_x = ceilf(max_x / texel_world_size) * texel_world_size;
    max_y = ceilf(max_y / texel_world_size) * texel_world_size;

    // Pull visible geometry away from the shadow-map UV edges (reduces PCF border leaks).
    float edge_margin = texel_world_size * 4.0f;
    min_x -= edge_margin;
    min_y -= edge_margin;
    max_x += edge_margin;
    max_y += edge_margin;

    Matrix4 light_proj = Matrix4Ortho(min_x, max_x, min_y, max_y, 0.1f, 2.0f * light_distance);
    *out_light_space = Matrix4Multiply(light_proj, light_view);
    *out_texel_world_size = texel_world_size;
}





// Builds the eight world-space corners of a camera frustum slice.
void BuildFrustumSliceCorners(Vector3 cam_pos, Vector3 cam_fwd, Vector3 cam_right, Vector3 cam_up, float aspect, float tan_half, float split_near, float split_far, Vector3 corners[8])
{
    float near_h = split_near * tan_half;
    float near_w = near_h * aspect;
    float far_h = split_far * tan_half;
    float far_w = far_h * aspect;

    Vector3 near_center = {
        cam_pos.x + cam_fwd.x * split_near,
        cam_pos.y + cam_fwd.y * split_near,
        cam_pos.z + cam_fwd.z * split_near
    };
    Vector3 far_center = {
        cam_pos.x + cam_fwd.x * split_far,
        cam_pos.y + cam_fwd.y * split_far,
        cam_pos.z + cam_fwd.z * split_far
    };

    corners[0] = (Vector3){ near_center.x - cam_right.x * near_w - cam_up.x * near_h,
                            near_center.y - cam_right.y * near_w - cam_up.y * near_h,
                            near_center.z - cam_right.z * near_w - cam_up.z * near_h };
    corners[1] = (Vector3){ near_center.x + cam_right.x * near_w - cam_up.x * near_h,
                            near_center.y + cam_right.y * near_w - cam_up.y * near_h,
                            near_center.z + cam_right.z * near_w - cam_up.z * near_h };
    corners[2] = (Vector3){ near_center.x + cam_right.x * near_w + cam_up.x * near_h,
                            near_center.y + cam_right.y * near_w + cam_up.y * near_h,
                            near_center.z + cam_right.z * near_w + cam_up.z * near_h };
    corners[3] = (Vector3){ near_center.x - cam_right.x * near_w + cam_up.x * near_h,
                            near_center.y - cam_right.y * near_w + cam_up.y * near_h,
                            near_center.z - cam_right.z * near_w + cam_up.z * near_h };
    corners[4] = (Vector3){ far_center.x - cam_right.x * far_w - cam_up.x * far_h,
                            far_center.y - cam_right.y * far_w - cam_up.y * far_h,
                            far_center.z - cam_right.z * far_w - cam_up.z * far_h };
    corners[5] = (Vector3){ far_center.x + cam_right.x * far_w - cam_up.x * far_h,
                            far_center.y + cam_right.y * far_w - cam_up.y * far_h,
                            far_center.z + cam_right.z * far_w - cam_up.z * far_h };
    corners[6] = (Vector3){ far_center.x + cam_right.x * far_w + cam_up.x * far_h,
                            far_center.y + cam_right.y * far_w + cam_up.y * far_h,
                            far_center.z + cam_right.z * far_w + cam_up.z * far_h };
    corners[7] = (Vector3){ far_center.x - cam_right.x * far_w + cam_up.x * far_h,
                            far_center.y - cam_right.y * far_w + cam_up.y * far_h,
                            far_center.z - cam_right.z * far_w + cam_up.z * far_h };
}