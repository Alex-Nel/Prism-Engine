#include "Prism.h"
#include <stddef.h>
#include <stdlib.h>


static PrismEngine engine;
static Window* main_window = NULL;
static bool is_running = false;
static bool mouse_captured = false;

// static float mainWindow_width = 0;
// static float mainWindow_height = 0;



bool Engine_Init(const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine.window_title = window_title;
    engine.window_width = window_width;
    engine.window_height = window_height;
    engine.target_fps = target_fps;


    // 1. Core Init
    Input_Init();
    Time_Init(engine.target_fps, Platform_GetTime, Platform_Delay);

    // 2. Platform Init
    main_window = Platform_Init(engine.window_title, engine.window_width, engine.window_height, api);
    if (!main_window)
    {
        Log_Error("Window failed to initialize.\n");
        return false;
    }

    void* proc_addr = NULL;
    if (api == GRAPHICS_API_OPENGL)
        proc_addr = Platform_GetProcAddress;
    
    // 3. Render Init (Injecting the OS loader)
    if (!Render_Init(api, Platform_GetProcAddress))
    {
        Platform_Shutdown(main_window);
        Log_Error("Renderer failed to initialize.\n");
        return false;
    }

    is_running = true;

    return true;
}





bool Engine_IsRunning()
{
    if (!is_running) return false;

    // 1. Advance the engine clock
    Time_Tick();

    // 2. Event Routing Loop
    Event e;
    while (Platform_PollEvents(&e))
    {
        // Feed the input manager
        Input_ProcessEvent(&e);

        // Route structural events to other modules!
        switch (e.type)
        {
            case EVENT_WINDOW_CLOSE:
                is_running = false;
                break;
                
            case EVENT_WINDOW_RESIZE:
                Render_SetViewport(0, 0, e.window_resize.width, e.window_resize.height);
                engine.window_width = e.window_resize.width;
                engine.window_height = e.window_resize.height;
                Log_Info("Window resized to: %d, %d\n", e.window_resize.width, e.window_resize.height);
                break;
                
            default:
                break;
        }
    }

    return is_running;
}





void Engine_RenderScene(Scene* scene)
{
    if (!scene) return;


    RenderPacket packet = {0};
    packet.global_light = scene->global_light;


    // --- 1. Harvest Point Lights from the ECS ---
    PointLightData active_lights[MAX_RESOURCES]; // (Make sure MAX_POINT_LIGHTS is defined in your header!)
    uint32_t light_count = 0;
    uint32_t light_mask = COMPONENT_TRANSFORM | COMPONENT_POINT_LIGHT;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if ((scene->component_masks[i] & light_mask) == light_mask)
        {    
            Transform* t = &scene->transforms[i];
            PointLightComponent* l = &scene->point_lights[i];
            
            // active_lights[light_count].position = t->local_position;
            active_lights[light_count].position = Transform_GetGlobalPosition(t);
            active_lights[light_count].color = l->color;
            active_lights[light_count].intensity = l->intensity;
            active_lights[light_count].constant = l->constant;
            active_lights[light_count].linear = l->linear;
            active_lights[light_count].quadratic = l->quadratic;
            
            light_count++;
            if (light_count >= MAX_RESOURCES) break; // Don't overflow the array!
        }
    }

    packet.point_lights = active_lights;
    packet.point_light_count = light_count;
    

    uint32_t cam_id = scene->main_camera_id;
    uint32_t cam_mask = COMPONENT_TRANSFORM | COMPONENT_CAMERA;

    if ((scene->component_masks[cam_id] & cam_mask) == cam_mask)
    {
        // THIS is where we get those variables!
        Transform* cam_transform = &scene->transforms[cam_id];
        CameraComponent* cam_comp = &scene->cameras[cam_id];

        Vector3 global_pos = Transform_GetGlobalPosition(cam_transform);

        // Now we can use them to build our View and Projection matrices
        Matrix4 view = Matrix4CreateView(global_pos, cam_transform->local_rotation);
        Matrix4 proj = Matrix4Perspective(
            cam_comp->fov,
            (float)(engine.window_width)/(float)(engine.window_height),
            cam_comp->nearZ,
            cam_comp->farZ
        );

        packet.view_matrix = view;
        packet.projection_matrix = proj;
        packet.camera_pos = global_pos;
        
        // Render_BeginFrame(view, proj, cam_transform->position, scene->global_light);
        // Render_BeginFrame(view, proj, cam_transform->position, scene->global_light, active_lights, light_count);
        Render_BeginFrame(&packet);
    }
    else
    {
        // Fallback: If no camera exists, just use identity matrices
        packet.view_matrix = Matrix4Identity();
        packet.projection_matrix = Matrix4Identity();
        packet.camera_pos = (Vector3){0.0f, 0.0f, 0.0f};

        // Render_BeginFrame(Matrix4Identity(), Matrix4Identity(), (Vector3){0.0f, 0.0f, 0.0f}, scene->global_light);
        // Render_BeginFrame(Matrix4Identity(), Matrix4Identity(), (Vector3){0.0f, 0.0f, 0.0f}, scene->global_light, active_lights, light_count);
        Render_BeginFrame(&packet);
    }

    // 2. The Extraction Loop
    // The Engine looks at the Scene data and hands it to the Renderer
    uint32_t required_mask = COMPONENT_TRANSFORM | COMPONENT_RENDER;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if ((scene->component_masks[i] & required_mask) == required_mask)
        {
            
            // Re-wrap the raw IDs back into strongly-typed Handles for the Renderer
            MeshHandle mesh = { scene->renderables[i].mesh_id };
            MaterialHandle mat_handle = { scene->renderables[i].material_id };
            Material* mat = Asset_GetMaterial(mat_handle);

            if (mat != NULL)
            {
                ShaderHandle shader = { mat->shader_id };
                TextureHandle texture = { mat->diffuse_texture_id };

                Render_Submit(
                    mesh,
                    shader,
                    texture,
                    mat->properties,
                    scene->transforms[i].world_matrix
                );
            }
        }
    }
}





void Engine_EndFrame()
{
    if (!is_running) return;

    // 1. Dispatch all queued draw calls to the GPU
    Render_EndFrame();

    // 2. Swap the OS window buffers to display the new frame
    Platform_SwapBuffers(main_window);

    // 3. Cycle the input arrays for the next frame
    Input_Update();
}





void Engine_Shutdown()
{
    Render_Shutdown();
    if (main_window)
    {
        Platform_Shutdown(main_window);
    }
}





void Engine_CaptureMouse()
{
    mouse_captured = true;
    Platform_SetRelativeMouseMode(main_window, mouse_captured);
    // mouse_captured = enabled;
    if (!mouse_captured)
        Platform_WarpMouse(main_window);
}





void Engine_ReleaseMouse()
{
    mouse_captured = false;
    Platform_SetRelativeMouseMode(main_window, mouse_captured);
    // mouse_captured = enabled;
    if (!mouse_captured)
        Platform_WarpMouse(main_window);
}





bool Engine_IsMouseCaptured()
{
    return mouse_captured;
}