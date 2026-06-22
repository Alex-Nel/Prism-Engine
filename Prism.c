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
    if (!scene) return;

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
        if (!scene->is_active_in_hierarchy[i]) continue;

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

    uint32_t required_mesh_mask = COMPONENT_TRANSFORM | COMPONENT_RENDER;

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
        packet.view_matrix = Matrix4CreateView(global_pos, cam_transform->local_rotation);
        packet.projection_matrix = Matrix4Perspective(
            cam_comp->fov,
            (float)(Platform_GetWindowWidth(engine.window))/(float)(Platform_GetWindowHeight(engine.window)),
            cam_comp->nearZ, cam_comp->farZ
        );
        packet.camera_pos = global_pos;

        packet.has_skybox = (cam_comp->clear_flags == CLEAR_COLOR_AND_DEPTH) ? scene->has_skybox : false;

        // Start pass
        Render_BeginFrame(engine.renderer, &packet);

        // Submit meshes for this camera only
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!scene->is_active_in_hierarchy[i])
                continue;

            if ((scene->component_masks[i] & required_mesh_mask) == required_mesh_mask)
            {
                RenderComponent* rc = &scene->renderables[i];

                if ((rc->layer_mask & cam_comp->culling_masks) == 0)
                    continue;

                if (rc->mesh && rc->material)
                {
                    Transform* t = &scene->transforms[i];

                    Matrix4* bone_ptr = NULL;

                    // Check if the entity has an animator or if it's parent has an animator
                    if (scene->component_masks[i] & COMPONENT_ANIMATOR)
                    {
                        bone_ptr = scene->animators[i].final_bone_matrices;
                    }
                    else if (t->parent_id != 0 && t->parent_id != ENTITY_NONE)
                    {
                        if (scene->component_masks[t->parent_id] & COMPONENT_ANIMATOR)
                        {
                            bone_ptr = scene->animators[t->parent_id].final_bone_matrices;
                        }
                    }


                    Render_Submit(engine.renderer, rc->mesh->gpu_handle, rc->material->shader->gpu_handle,
                                    rc->material->diffuse_texture->gpu_handle, rc->material->properties,
                                    t->world_matrix, bone_ptr);
                }
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
                                  Matrix4Identity(), NULL);
                }
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