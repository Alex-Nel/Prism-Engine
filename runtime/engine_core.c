#include "engine_runtime.h"



// Forward declare OnModalEvent function
static void Engine_OnModalEvent(void* userdata);



// Initializes all engine systems
bool Engine_Init(PrismEngine* engine, const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine->target_fps = target_fps;
    engine->active_scene = NULL;

    // Platform Init
    engine->window = Platform_Init(window_title, window_width, window_height, api);
    if (!engine->window && api != GRAPHICS_API_NONE)
    {
        Log_Error("Window failed to initialize.\n");
        return false;
    }

    // Register global modal window event callback
    Platform_SetEventWatchCallback(Engine_OnModalEvent, engine);

    // Get the Procedure address if OpenGL is used
    void* proc_addr;
    if (api == GRAPHICS_API_OPENGL)
        proc_addr = Platform_GetProcAddress;
    else
        proc_addr = NULL;
    
    // Render Init
    Renderer* renderer = Render_Init(api, proc_addr, window_width, window_height);
    if (!renderer)
    {
        Platform_Shutdown(engine->window);
        Log_Error("Renderer failed to initialize.\n");
        return false;
    }
    engine->renderer = renderer;

    // Initialize default renderer settings
    RendererSettings default_settings = {
        .enable_ssao = false,
        .shadow_map_resolution = SHADOW_MAP_RESOLUTION,
        .gamma = 2.2f
    };
    Render_SetSettings(engine->renderer, &default_settings);

    // Set renderer clear color to pure white
    Render_SetClearColor(renderer, 0.8f, 0.8f, 0.8f, 1.0f);

    // Initialize UI
    UI_Init();
    UI_SetClipboardCallbacks(Platform_SetClipboardText, Platform_GetClipboardText, Platform_FreeClipboardText);
    Render_UIinit(engine->renderer, UI_GetContext());

    // Core modules init
    Input_Init();
    Audio_Init();
    Asset_Init(renderer);
    Time_Init(engine->target_fps, Platform_GetTime, Platform_Delay);

    engine->is_running = true;
    engine->is_simulating = true;
    engine->accumulator = 0.0f;

    return true;
}





// Shuts down all systems
void Engine_Shutdown(PrismEngine* engine)
{
    UI_Shutdown();
    Audio_Shutdown();
    Render_UIShutdown(engine->renderer);
    Render_Shutdown(engine->renderer);

    if (engine->window)
        Platform_Shutdown(engine->window);
}





// Sets the custom callback function
void Engine_SetPreUpdateCallback(PrismEngine* engine, EngineUpdateCallback callback)
{
    engine->pre_update_callback = callback;
}





// Toggles physics and script execution
void Engine_SetSimulationMode(PrismEngine* engine, bool is_simulating)
{
    engine->is_simulating = is_simulating;
}





// Forwards the UI in time
static void Engine_TickRetainedUI(PrismEngine* engine, Scene* active_scene)
{
    uint32_t w = Platform_GetWindowWidth(engine->window);
    uint32_t h = Platform_GetWindowHeight(engine->window);
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
    Input_GetMousePosition(&mouse_x, &mouse_y);

    RetainedUI_PreUpdate(active_scene, w, h, mouse_x, mouse_y, Engine_IsMouseCaptured(engine));
}





// Draws the retained UI
static void Engine_DrawRetainedUI(PrismEngine* engine, Scene* active_scene)
{
    if (!engine->renderer || !active_scene)
        return;

    uint32_t w = Platform_GetWindowWidth(engine->window);
    uint32_t h = Platform_GetWindowHeight(engine->window);
    
    RetainedUI_UpdateLayout(active_scene, w, h);
    RetainedUI_BuildOverlay(active_scene);
    
    Render_DrawOverlay(engine->renderer, &g_ui_state.draw_list, w, h);
}





// Updates whether the engine should accept text input or not
static void Engine_UpdateTextInput(PrismEngine* engine)
{
    bool wants_text_input = UI_WantsTextInput();

    // The platform already knows if it is accepting text input, so only tell it about changes
    if (wants_text_input == Platform_IsTextInputActive(engine->window))
        return;

    if (wants_text_input)
        Platform_StartTextInput(engine->window);
    else
        Platform_StopTextInput(engine->window);
}





// Draws the immediate mode UI
static void Engine_DrawImmediateUI(PrismEngine* engine)
{
    Render_UIRender(engine->renderer, UI_GetContext(), Platform_GetWindowWidth(engine->window), Platform_GetWindowHeight(engine->window));
}





// A function to process window events without pausing main loop
static void Engine_OnModalEvent(void* userdata)
{
    PrismEngine* engine = (PrismEngine*)userdata;
    if (!engine || !engine->active_scene)
        return;

    Scene* active_scene = engine->active_scene;

    // Force the renderer to update its viewport immediately
    uint32_t w = Platform_GetWindowWidth(engine->window);
    uint32_t h = Platform_GetWindowHeight(engine->window);
    
    if (w > 0 && h > 0)
        Render_SetViewport(engine->renderer, 0, 0, w, h);

    // Tick the time to prevent physics/animation explosions when we let go
    Time_Tick();

    engine->accumulator += Time_DeltaTime();

    if (engine->is_simulating)
    {
        float fixed_dt = Time_FixedDeltaTime();
        while (engine->accumulator >= fixed_dt)
        {
            Scene_FixedUpdate(active_scene);
            engine->accumulator -= fixed_dt;
        }
    }
    else
    {
        // Don't accumulate time if not simulating
        engine->accumulator = 0.0f;
    }

    Engine_TickRetainedUI(engine, active_scene);

    if (engine->is_simulating)
    {
        // Update scripts/animations
        Scene_Update(active_scene);
    }
    else
    {
        // If not simulating, only update visual entities
        Scene_UpdateTransforms(active_scene);
        Scene_UpdateBoneAttachments(active_scene);
        Scene_UpdateSkinnedMeshBounds(active_scene);
        Scene_UpdateLineRenderers(active_scene);
    }

    Engine_UpdateTextInput(engine);

    // Render and Swap Buffers directly
    if (!Platform_IsWindowMinimized(engine->window))
    {
        Engine_RenderScene(engine, active_scene);
        Engine_DrawRetainedUI(engine, active_scene);
        if (engine->modal_callback)
            engine->modal_callback(engine->modal_userdata);
        Render_UIRender(engine->renderer, UI_GetContext(), w, h);
        Platform_SwapBuffers(engine->window);
    }
}





void Engine_SetModalCallback(PrismEngine* engine, EngineModalCallback callback, void* userdata)
{
    engine->modal_callback = callback;
    engine->modal_userdata = userdata;
}





// Updates the engines state
void Engine_Update(PrismEngine* engine, Scene* active_scene)
{
    if (!active_scene)
        return;

    engine->active_scene = active_scene;

    Time_Tick();
    engine->accumulator += Time_DeltaTime();

    UI_InputBegin();

    Event e;
    while (Platform_PollEvents(&e))
    {
        bool ui_handled = false;
        if (!Engine_IsMouseCaptured(engine))
            ui_handled = UI_ProcessEvent(&e);

        if (!ui_handled)
            Input_ProcessEvent(&e);
        
        if (e.type == EVENT_WINDOW_CLOSE)
        {
            engine->is_running = false;
        }
        else if (e.type == EVENT_WINDOW_RESIZE)
        {
            Render_SetViewport(engine->renderer, 0, 0, e.window_resize.width, e.window_resize.height);
            Platform_SetWindowSize(engine->window, e.window_resize.width, e.window_resize.height);
        }
        else if (e.type == EVENT_WINDOW_MINIMIZED)
        {
            Platform_SetWindowMinimized(engine->window, true);
        }
        else if (e.type == EVENT_WINDOW_RESTORED)
        {
            Platform_SetWindowMinimized(engine->window, false);
        }
    }

    UI_InputEnd();

    // If the API registered a custom callback, call it
    if (engine->pre_update_callback != NULL)
        engine->pre_update_callback();

    if (engine->is_simulating)
    {
        // Update accumulator and fixed updates
        float fixed_dt = Time_FixedDeltaTime();
        while (engine->accumulator >= fixed_dt)
        {
            Scene_FixedUpdate(active_scene);
            engine->accumulator -= fixed_dt;
        }
    }
    else
    {
        // Don't accumulate time if not simulating
        engine->accumulator = 0.0f;
    }

    Engine_TickRetainedUI(engine, active_scene);

    if (engine->is_simulating)
    {
        // Update scene, physics, and UI
        Scene_Update(active_scene);
    }
    else
    {
        // If not simulating, only update visual entities
        Scene_UpdateTransforms(active_scene);
        Scene_UpdateBoneAttachments(active_scene);
        Scene_UpdateSkinnedMeshBounds(active_scene);
        Scene_UpdateLineRenderers(active_scene);
    }

    Engine_UpdateTextInput(engine);
}





// Renders everything in a scene including overlays and UI
void Engine_Render(PrismEngine* engine, Scene* active_scene)
{
    if (!active_scene)
        return;

    // Render the scene if the window is not minimized
    if (!Platform_IsWindowMinimized(engine->window))
    {
        // Render scene
        Engine_RenderScene(engine, active_scene);

        // Render UI
        Engine_DrawRetainedUI(engine, active_scene);
        Engine_DrawImmediateUI(engine);
    }

    // Process destroy queue
    Scene_ProcessDestroyQueue(active_scene);
    
    // Swap Buffers & Reset Input arrays
    Engine_EndFrame(engine);
}





// Runs the engine, updates and renders the scene
void Engine_Run(PrismEngine* engine, Scene* active_scene)
{
    if (!active_scene)
    {
        Log_Error("Cannot run engine without an active scene");
        return;
    }

    engine->active_scene = active_scene;

    Log_Info("Running Scene");

    engine->accumulator = 0.0f;

    while (engine->is_running)
    {
        Engine_Update(engine, active_scene);
        Engine_Render(engine, active_scene);
    }
}





// *Deprecated* - Use Engine_Run instead
// Continues running the engine. Returns true if it's still running
bool Engine_IsRunning(PrismEngine* engine)
{
    if (!engine->is_running) return false;

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
                engine->is_running = false;
                break;
                
            case EVENT_WINDOW_RESIZE:
                Render_SetViewport(engine->renderer, 0, 0, e.window_resize.width, e.window_resize.height);
                Platform_SetWindowSize(engine->window, e.window_resize.width, e.window_resize.height);
                Log_Info("Window resized to: %d, %d\n", e.window_resize.width, e.window_resize.height);
                break;
                
            default:
                break;
        }
    }

    return engine->is_running;
}





// Renders the frame, swaps the buffers and updates the input
void Engine_EndFrame(PrismEngine* engine)
{
    if (!engine->is_running)
        return;

    if (!Platform_IsWindowMinimized(engine->window))
    {
        // Swap the OS window buffers to display the new frame
        Platform_SwapBuffers(engine->window);
    }

    // Cycle the input arrays for the next frame
    Input_Update();
}