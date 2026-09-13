#include "engine_runtime.h"



// Forward declare OnModalEvent function
static void Engine_OnModalEvent(void* userdata);





// Initializes all engine systems
bool Engine_Init(PrismEngine* engine, const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine->target_fps = target_fps;
    engine->window = NULL;
    engine->renderer = NULL;
    engine->active_scene = NULL;
    engine->render_thread_ready = false;
    engine->modal_update_active = false;
    engine->modal_update_performed = false;


    if (api != GRAPHICS_API_NONE)
        Render_ConfigurePlatformSurface(api);

    // Platform Init
    engine->window = Platform_Init(window_title, window_width, window_height, api);
    if (!engine->window && api != GRAPHICS_API_NONE)
    {
        Log_Error("Window failed to initialize.\n");
        return false;
    }

    // Register global modal window event callback
    Platform_SetEventWatchCallback(Engine_OnModalEvent, engine);

    void* native_window = NULL;
    if (engine->window)
        native_window = Platform_GetNativeWindow(engine->window);
    
    // Render Init
    Renderer* renderer = Render_Init(api, native_window, window_width, window_height);
    if (!renderer)
    {
        Platform_Shutdown(engine->window);
        Log_Error("Renderer failed to initialize.\n");
        return false;
    }
    engine->renderer = renderer;

    // Initialize renderer settings from the backend defaults
    RendererSettings default_settings = Render_GetSettings(engine->renderer);
    default_settings.enable_ssao = false;
    default_settings.gamma = 2.2f;
    Render_SetSettings(engine->renderer, &default_settings);

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
    engine->modal_update_active = false;
    engine->modal_update_performed = false;
    engine->render_frame_counter = 0;
    engine->last_applied_render_frame = 0;
    engine->pending_frame_width = 0;
    engine->pending_frame_height = 0;
    engine->pending_framebuffer_resize = false;
    engine->render_thread = NULL;
    engine->render_start_mutex = NULL;
    engine->render_start_condition = NULL;
    engine->render_thread_ready = false;
    engine->render_thread_failed = false;

    // Start the frame handoff only after all main-thread renderer setup is complete
    if (!RenderFrameQueue_Init(&engine->frame_queue) || !EngineRenderThread_Start(engine))
    {
        Log_Error("Render thread failed to initialize.");
        EngineRenderThread_Stop(engine);
        RenderFrameQueue_Shutdown(&engine->frame_queue);
        
        UI_Shutdown();
        Audio_Shutdown();
        Platform_Shutdown(engine->window);
        
        return false;
    }

    return true;
}





// Shuts down all systems
void Engine_Shutdown(PrismEngine* engine)
{
    if (!engine)
        return;

    Audio_Shutdown();

    // Wake the worker, drain submitted frames, and shutdown GPU state on its owner thread
    EngineRenderThread_Stop(engine);

    // Results are no longer useful during shutdown, but their slots must be released
    uint32_t completed_slot = 0;
    const RenderFrameResult* discarded_result = NULL;
    while (RenderFrameQueue_AcquireCompleted(&engine->frame_queue, false, &completed_slot, &discarded_result))
    {
        RenderFrameQueue_ReleaseCompleted(&engine->frame_queue, completed_slot);
    }
    UI_Shutdown();

    RenderFrameQueue_Shutdown(&engine->frame_queue);

    Platform_Shutdown(engine->window);
    engine->window = NULL;
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





// Records a pending framebuffer resize from the main thread. GPU resources are not touched here.
void Engine_NotifyFramebufferResize(PrismEngine* engine, uint32_t width, uint32_t height)
{
    if (!engine || width == 0 || height == 0)
        return;
    
    engine->pending_frame_width = width;
    engine->pending_frame_height = height;
    engine->pending_framebuffer_resize = true;
}





// Applies a pending resize on the render path before GPU work begins.
void Engine_ApplyPendingFramebufferResize(PrismEngine* engine)
{
    if (!engine || !engine->renderer || !engine->pending_framebuffer_resize)
        return;

    Render_Resize(engine->renderer, engine->pending_frame_width, engine->pending_frame_height);
    engine->pending_framebuffer_resize = false;
}





// Advances simulation and visual scene state without polling platform events
static void Engine_AdvanceSceneState(PrismEngine* engine, Scene* active_scene, bool update_text_input)
{
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
        engine->accumulator = 0.0f;
    }

    Engine_TickRetainedUI(engine, active_scene);
    
    if (engine->is_simulating)
    {
        Scene_Update(active_scene);
    }
    else
    {
        Scene_UpdateTransforms(active_scene);
        Scene_UpdateBoneAttachments(active_scene);
        Scene_UpdateSkinnedMeshBounds(active_scene);
        Scene_UpdateLineRenderers(active_scene);
    }

    if (update_text_input)
        Engine_UpdateTextInput(engine);
}





// Advances and submits the engine while a native move or resize loop blocks Engine_Run
static void Engine_OnModalEvent(void* userdata)
{
    PrismEngine* engine = (PrismEngine*)userdata;
    if (!engine || !engine->window)
        return;

    uint32_t w = Platform_GetWindowWidth(engine->window);
    uint32_t h = Platform_GetWindowHeight(engine->window);
    
    if (w > 0 && h > 0)
        Engine_NotifyFramebufferResize(engine, w, h);

    if (!engine->renderer || !engine->render_thread_ready)
        return;

    // Nested watcher calls must never re-enter scene or UI code
    if (engine->modal_update_active || !engine->active_scene || Platform_IsWindowMinimized(engine->window))
    {
        RenderFrameQueue_RequestRedraw(&engine->frame_queue, w, h);
        return;
    }

    engine->modal_update_active = true;
    
    // The normal loop is blocked inside the native move/resize loop, so tick here
    Time_Tick();
    engine->accumulator += Time_DeltaTime();
    EngineRenderThread_PumpCompletions(engine);
    Engine_AdvanceSceneState(engine, engine->active_scene, false);
    
    if (engine->modal_callback)
        engine->modal_callback(engine->modal_userdata);
    
    // Submit only when a slot is immediately available; never block the window callback
    if (RenderFrameQueue_HasFreeSlot(&engine->frame_queue))
    {
        Engine_RenderScene(engine, engine->active_scene);
        Scene_ProcessDestroyQueue(engine->active_scene);
    }
    else
    {
        RenderFrameQueue_RequestRedraw(&engine->frame_queue, w, h);
    }
    
    engine->modal_update_performed = true;
    engine->modal_update_active = false;
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
            Platform_SetWindowSize(engine->window, e.window_resize.width, e.window_resize.height);
            Engine_NotifyFramebufferResize(engine, e.window_resize.width, e.window_resize.height);
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

    // Modal events already advanced this iteration while the native loop was blocking
    if (engine->modal_update_performed)
    {
        engine->modal_update_performed = false;
        Engine_UpdateTextInput(engine);
        return;
    }

    // If the API registered a custom callback, call it
    if (engine->pre_update_callback != NULL)
        engine->pre_update_callback();

    Engine_AdvanceSceneState(engine, active_scene, true);
}





// Renders everything in a scene including overlays and UI
void Engine_Render(PrismEngine* engine, Scene* active_scene)
{
    if (!active_scene)
        return;

    engine->active_scene = active_scene;

    // Reclaim completed slots before attempting to build another bounded snapshot
    EngineRenderThread_PumpCompletions(engine);
    while (!RenderFrameQueue_HasFreeSlot(&engine->frame_queue))
    {
        if (!EngineRenderThread_ApplyOneCompletion(engine, true))
            break;
    }

    // Render the scene if the window is not minimized
    if (!Platform_IsWindowMinimized(engine->window))
    {
        // Build and submit world plus UI snapshots. GPU work happens asynchronously.
        Engine_RenderScene(engine, active_scene);
    }
    else
    {
        // Do not let immediate UI commands accumulate while rendering is paused.
        OverlayDrawList discarded_ui = {0};
        UI_BuildDrawList(&discarded_ui);
        OverlayDrawList_Free(&discarded_ui);
    }

    // Process destroy queue
    Scene_ProcessDestroyQueue(active_scene);
    
    // Cycle input state. Present is owned by the render thread.
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
                Platform_SetWindowSize(engine->window, e.window_resize.width, e.window_resize.height);
                Engine_NotifyFramebufferResize(engine, e.window_resize.width, e.window_resize.height);
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

    // Cycle the input arrays for the next frame
    Input_Update();
}