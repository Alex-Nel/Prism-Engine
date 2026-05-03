#include "Prism.h"
#include <stddef.h>
#include <stdlib.h>


// Internal states
static PrismEngine engine;
static bool mouse_captured = false;



// Initializes all engine systems
bool Engine_Init(const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine.target_fps = target_fps;

    // Core Init
    Input_Init();
    Asset_Init();
    Time_Init(engine.target_fps, Platform_GetTime, Platform_Delay);

    // Platform Init
    engine.window = Platform_Init(window_title, window_width, window_height, api);
    if (!engine.window)
    {
        Log_Error("Window failed to initialize.\n");
        return false;
    }

    // Get the Procedure address if OpenGL is used
    void* proc_addr;
    if (api == GRAPHICS_API_OPENGL)
        proc_addr = Platform_GetProcAddress;
    else
        proc_addr = NULL;
    
    // Render Init
    if (!Render_Init(api, proc_addr))
    {
        Platform_Shutdown(engine.window);
        Log_Error("Renderer failed to initialize.\n");
        return false;
    }

    // Set renderer clear color to pure white
    Engine_SetClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    engine.is_running = true;

    return true;
}





// Get a pointer to the main window
Window* Engine_GetMainWindow()
{
    return engine.window;
}





// Runs the engine, updates and renders the scene
void Engine_Run(Scene* active_scene)
{
    if (!active_scene) {
        Log_Error("Cannot run engine without an active scene");
        return;
    }

    while (engine.is_running)
    {
        // Advance the engine clock
        Time_Tick();
        
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
                Render_SetViewport(0, 0, e.window_resize.width, e.window_resize.height);
                SetWindowSize(engine.window, e.window_resize.width, e.window_resize.height);
            }
        }

        // Update scene and physics
        Scene_Update(active_scene);

        // Render scene
        Render_Clear();
        Engine_RenderScene(active_scene);
        
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
                Render_SetViewport(0, 0, e.window_resize.width, e.window_resize.height);
                SetWindowSize(engine.window, e.window_resize.width, e.window_resize.height);
                Log_Info("Window resized to: %d, %d\n", e.window_resize.width, e.window_resize.height);
                break;
                
            default:
                break;
        }
    }

    return engine.is_running;
}





// Sets the clear color of the renderer
void Engine_SetClearColor(float r, float g, float b, float a)
{
    Render_SetClearColor(r, g, b, a);
}





// Renders a specified scene
void Engine_RenderScene(Scene* scene)
{
    if (!scene) return;


    // Make an empty render packet to send to the renderer
    RenderPacket packet = {0};
    packet.global_light = scene->global_light;


    // Get all Point Lights from the ECS
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
            
            active_lights[light_count].position = Transform_GetGlobalPosition(t);
            active_lights[light_count].color = l->color;
            active_lights[light_count].intensity = l->intensity;
            active_lights[light_count].constant = l->constant;
            active_lights[light_count].linear = l->linear;
            active_lights[light_count].quadratic = l->quadratic;
            
            light_count++;
            if (light_count >= MAX_RESOURCES)
                break;
        }
    }

    packet.point_lights = active_lights;
    packet.point_light_count = light_count;
    

    uint32_t cam_id = scene->main_camera_id;
    uint32_t cam_mask = COMPONENT_TRANSFORM | COMPONENT_CAMERA;


    if ((scene->component_masks[cam_id] & cam_mask) == cam_mask)
    {
        Transform* cam_transform = &scene->transforms[cam_id];
        CameraComponent* cam_comp = &scene->cameras[cam_id];

        Vector3 global_pos = Transform_GetGlobalPosition(cam_transform);

        // Use an objects transform and global position to make the world matrix
        Matrix4 view = Matrix4CreateView(global_pos, cam_transform->local_rotation);
        Matrix4 proj = Matrix4Perspective(
            cam_comp->fov,
            // (float)(engine.window_width)/(float)(engine.window_height),
            (float)(GetWindowWidth(engine.window))/(float)(GetWindowHeight(engine.window)),
            cam_comp->nearZ,
            cam_comp->farZ
        );

        packet.view_matrix = view;
        packet.projection_matrix = proj;
        packet.camera_pos = global_pos;

        Render_BeginFrame(&packet);
    }
    else
    {
        // If no camera exists, use identity matrices
        packet.view_matrix = Matrix4Identity();
        packet.projection_matrix = Matrix4Identity();
        packet.camera_pos = (Vector3){0.0f, 0.0f, 0.0f};

        Render_BeginFrame(&packet);
    }


    // This loop looks at the Scene data and sends it to the Renderer
    uint32_t required_mask = COMPONENT_TRANSFORM | COMPONENT_RENDER;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if ((scene->component_masks[i] & required_mask) == required_mask)
        {
            // Re-wrap the raw IDs back into handles for the Renderer
            MeshHandle mesh = { scene->renderables[i].mesh_id };
            MaterialHandle mat_handle = { scene->renderables[i].material_id };
            Material* mat = Asset_GetMaterial(mat_handle);

            if (mat != NULL)
            {
                ShaderHandle shader = { mat->shader_id };
                TextureHandle texture = { mat->diffuse_texture_id };

                // Submit all information to the renderer
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





// Renders the frame, swaps the buffers and updates the input
void Engine_EndFrame()
{
    if (!engine.is_running) return;

    // Dispatch all queued draw calls to the GPU
    Render_EndFrame();

    // Swap the OS window buffers to display the new frame
    Platform_SwapBuffers(engine.window);

    // Cycle the input arrays for the next frame
    Input_Update();
}





// Shuts down the renderer and platform
void Engine_Shutdown()
{
    Render_Shutdown();
    if (engine.window)
    {
        Platform_Shutdown(engine.window);
    }
}










// Captures the mouse to the window
void Engine_CaptureMouse()
{
    mouse_captured = true;
    Platform_SetRelativeMouseMode(engine.window, mouse_captured);
    if (!mouse_captured)
        Platform_WarpMouse(engine.window);
}





// Releases the mouse from the window
void Engine_ReleaseMouse()
{
    mouse_captured = false;
    Platform_SetRelativeMouseMode(engine.window, mouse_captured);
    if (!mouse_captured)
        Platform_WarpMouse(engine.window);
}





// Returns if the mouse is captured
bool Engine_IsMouseCaptured()
{
    return mouse_captured;
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