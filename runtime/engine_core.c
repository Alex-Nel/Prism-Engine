#include "engine_runtime.h"



// Forward declare OnModalEvent function
static void Engine_OnModalEvent(void* userdata);





// Wakes the render worker when a forwarded renderer command is available
static void Engine_WakeRenderThread(void* userdata)
{
    PrismEngine* engine = (PrismEngine*)userdata;
    if (engine)
        RenderFrameQueue_Wake(&engine->frame_queue);
}





// Owns the graphics context and consumes submitted frames and renderer commands
static int Engine_RenderThreadMain(void* userdata)
{
    PrismEngine* engine = (PrismEngine*)userdata;
    if (!engine || !engine->renderer)
        return -1;

    // Transfer the renderer context to this worker before accepting any work
    Render_SetRenderThreadID(engine->renderer, Platform_GetCurrentThreadID());
    bool context_ready = Render_MakeCurrent(engine->renderer);
    
    Platform_LockMutex(engine->render_start_mutex);
    engine->render_thread_ready = context_ready;
    engine->render_thread_failed = !context_ready;
    
    Platform_BroadcastCondition(engine->render_start_condition);
    Platform_UnlockMutex(engine->render_start_mutex);
    
    if (!context_ready)
        return -1;
    
    // Submitted frames take priority so later resource changes cannot overtake them
    for (;;)
    {
        uint32_t slot_index = 0;
        const RenderFrame* frame = NULL;
        bool is_redraw = false;
        uint32_t output_width = 0;
        uint32_t output_height = 0;

        if (RenderFrameQueue_WaitRead( &engine->frame_queue, &slot_index, &frame, &is_redraw, &output_width, &output_height))
        {
            RenderFrameResult result = {0};
            result.frame_id = frame->frame_id;

            (void)is_redraw;
            Render_Resize(engine->renderer, output_width, output_height);
            Render_DrawFrame(engine->renderer, frame);
            Render_DrawOverlay(engine->renderer, &frame->retained_ui, output_width, output_height);
            Render_DrawOverlay(engine->renderer, &frame->immediate_ui, output_width, output_height);
            result.probe_result_count = Render_GetProbeResults(
                engine->renderer, result.probe_results, RENDER_FRAME_MAX_PROBES);
            Render_Present(engine->renderer);
            RenderFrameQueue_CompleteRead(&engine->frame_queue, slot_index, &result);
            continue;
        }

        // A wake without a frame means a synchronous resource/settings call is waiting
        while (Render_ProcessPendingCommand(engine->renderer)) { }

        if (RenderFrameQueue_IsStopping(&engine->frame_queue))
            break;
    }

    // GPU-side shutdown must happen before this thread releases its context
    Render_UIShutdown(engine->renderer);
    Render_DisableThreadDispatch(engine->renderer);
    Render_Shutdown(engine->renderer);
    
    return 0;
}





// Starts the render worker and waits until it owns the graphics context
static bool Engine_StartRenderThread(PrismEngine* engine)
{
    engine->render_start_mutex = Platform_CreateMutex();
    engine->render_start_condition = Platform_CreateCondition();
    if (!engine->render_start_mutex || !engine->render_start_condition)
        return false;

    // Enable command forwarding before the main thread gives up the context
    if (!Render_EnableThreadDispatch(engine->renderer, Engine_WakeRenderThread, engine))
        return false;
    
    Render_ReleaseCurrent(engine->renderer);
    engine->render_thread = Platform_CreateThread(Engine_RenderThreadMain, "PrismRender", engine);
    if (!engine->render_thread)
        return false;
    
    // Initialization cannot succeed until MakeCurrent has completed on the worker
    Platform_LockMutex(engine->render_start_mutex);
    while (!engine->render_thread_ready && !engine->render_thread_failed)
        Platform_WaitCondition(engine->render_start_condition, engine->render_start_mutex);
    
    bool ready = engine->render_thread_ready && !engine->render_thread_failed;
    Platform_UnlockMutex(engine->render_start_mutex);
    
    return ready;
}





// Applies one completed render result and releases its frame slot
static bool Engine_ApplyOneRenderCompletion(PrismEngine* engine, bool wait)
{
    uint32_t slot_index = 0;
    const RenderFrameResult* result = NULL;
    if (!RenderFrameQueue_AcquireCompleted(&engine->frame_queue, wait, &slot_index, &result))
        return false;

    Scene* result_scene = (Scene*)result->scene_identity;
    if (result_scene && result_scene == engine->active_scene && result->frame_id > engine->last_applied_render_frame)
    {
        Engine_ApplyFrameResults(engine, result_scene, result);
        engine->last_applied_render_frame = result->frame_id;
    }

    RenderFrameQueue_ReleaseCompleted(&engine->frame_queue, slot_index);
    return true;
}





// Applies every render completion currently available without blocking
static void Engine_PumpRenderCompletions(PrismEngine* engine)
{
    while (Engine_ApplyOneRenderCompletion(engine, false)) { }
}





// Initializes all engine systems
bool Engine_Init(PrismEngine* engine, const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine->target_fps = target_fps;
    engine->active_scene = NULL;

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
    if (!RenderFrameQueue_Init(&engine->frame_queue) || !Engine_StartRenderThread(engine))
    {
        Log_Error("Render thread failed to initialize.");
        RenderFrameQueue_RequestStop(&engine->frame_queue);
        if (engine->render_thread)
            Platform_JoinThread(engine->render_thread, NULL);

        Render_DisableThreadDispatch(engine->renderer);
        Render_MakeCurrent(engine->renderer);
        Render_UIShutdown(engine->renderer);
        Render_Shutdown(engine->renderer);
        
        engine->renderer = NULL;
        RenderFrameQueue_Shutdown(&engine->frame_queue);
        
        if (engine->render_start_condition)
            Platform_DestroyCondition(engine->render_start_condition);
        if (engine->render_start_mutex)
            Platform_DestroyMutex(engine->render_start_mutex);
        
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
    RenderFrameQueue_RequestStop(&engine->frame_queue);
    if (engine->render_thread)
    {
        Platform_JoinThread(engine->render_thread, NULL);
        engine->render_thread = NULL;
    }
    engine->renderer = NULL;

    // Results are no longer useful during shutdown, but their slots must be released
    uint32_t completed_slot = 0;
    const RenderFrameResult* discarded_result = NULL;
    while (RenderFrameQueue_AcquireCompleted(&engine->frame_queue, false, &completed_slot, &discarded_result))
    {
        RenderFrameQueue_ReleaseCompleted(&engine->frame_queue, completed_slot);
    }
    UI_Shutdown();

    RenderFrameQueue_Shutdown(&engine->frame_queue);

    if (engine->render_start_condition)
        Platform_DestroyCondition(engine->render_start_condition);
    if (engine->render_start_mutex)
        Platform_DestroyMutex(engine->render_start_mutex);

    engine->render_start_condition = NULL;
    engine->render_start_mutex = NULL;

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





// A function to process window events without pausing main loop
static void Engine_OnModalEvent(void* userdata)
{
    PrismEngine* engine = (PrismEngine*)userdata;
    if (!engine || !engine->window)
        return;

    uint32_t w = Platform_GetWindowWidth(engine->window);
    uint32_t h = Platform_GetWindowHeight(engine->window);
    
    if (w > 0 && h > 0)
        Engine_NotifyFramebufferResize(engine, w, h);

    RenderFrameQueue_RequestRedraw(&engine->frame_queue, w, h);
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

    engine->active_scene = active_scene;

    // Reclaim completed slots before attempting to build another bounded snapshot
    Engine_PumpRenderCompletions(engine);
    while (!RenderFrameQueue_HasFreeSlot(&engine->frame_queue))
    {
        if (!Engine_ApplyOneRenderCompletion(engine, true))
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