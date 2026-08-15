#include "engine_runtime.h"


static OverlayDrawList s_retained_ui_draw_list;



// Initializes all engine systems
bool Engine_Init(PrismEngine* engine, const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api)
{
    engine->target_fps = target_fps;

    // Platform Init
    engine->window = Platform_Init(window_title, window_width, window_height, api);
    if (!engine->window && api != GRAPHICS_API_NONE)
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

    if (!s_retained_ui_draw_list.vertices)
        OverlayDrawList_Init(&s_retained_ui_draw_list);
    else
        OverlayDrawList_Reset(&s_retained_ui_draw_list);

    engine->is_running = true;
    engine->accumulator = 0.0f;

    return true;
}





// Shuts down all systems
void Engine_Shutdown(PrismEngine* engine)
{
    OverlayDrawList_Free(&s_retained_ui_draw_list);
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
    RetainedUI_BuildOverlay(active_scene, &s_retained_ui_draw_list);
    
    Render_DrawOverlay(engine->renderer, &s_retained_ui_draw_list, w, h);
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





// A struct to pass multiple pieces of data to the event watch callback
typedef struct ModalContext {
    PrismEngine* engine;
    Scene* active_scene;
} ModalContext;



// A function to process window events without pausing main loop
static void Engine_OnModalEvent(void* userdata)
{
    ModalContext* ctx = (ModalContext*)userdata;
    PrismEngine* engine = ctx->engine;
    Scene* active_scene = ctx->active_scene;
    if (!active_scene || !engine)
        return;

    // Force the renderer to update its viewport immediately
    uint32_t w = Platform_GetWindowWidth(engine->window);
    uint32_t h = Platform_GetWindowHeight(engine->window);
    
    if (w > 0 && h > 0)
        Render_SetViewport(engine->renderer, 0, 0, w, h);

    // Tick the time to prevent physics/animation explosions when we let go
    Time_Tick();

    engine->accumulator += Time_DeltaTime();

    float fixed_dt = Time_FixedDeltaTime();
    while (engine->accumulator >= fixed_dt)
    {
        Scene_FixedUpdate(active_scene);
        engine->accumulator -= fixed_dt;
    }

    Engine_TickRetainedUI(engine, active_scene);

    // Update scripts/animations
    Scene_Update(active_scene);

    Engine_UpdateTextInput(engine);

    // Render and Swap Buffers directly
    Engine_RenderScene(engine, active_scene);
    Engine_DrawRetainedUI(engine, active_scene);
    Render_UIRender(engine->renderer, UI_GetContext(), w, h);
    Platform_SwapBuffers(engine->window);
}





// Runs the engine, updates and renders the scene
void Engine_Run(PrismEngine* engine, Scene* active_scene)
{
    if (!active_scene)
    {
        Log_Error("Cannot run engine without an active scene");
        return;
    }

    ModalContext modal_ctx = { engine, active_scene };
    Platform_SetEventWatchCallback(Engine_OnModalEvent, &modal_ctx);

    Log_Info("Running Scene");

    engine->accumulator = 0.0f;

    while (engine->is_running)
    {
        // Advance the engine clock
        Time_Tick();

        engine->accumulator += Time_DeltaTime();

        UI_InputBegin();
        
        // Poll through events
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
        }

        UI_InputEnd();

        // If the API registered a custom callback, call it
        if (engine->pre_update_callback != NULL)
            engine->pre_update_callback();

        // Update accumulator and fixed updates
        float fixed_dt = Time_FixedDeltaTime();
        while (engine->accumulator >= fixed_dt)
        {
            Scene_FixedUpdate(active_scene);
            engine->accumulator -= fixed_dt;
        }

        Engine_TickRetainedUI(engine, active_scene);

        // Update scene and physics
        Scene_Update(active_scene);

        Engine_UpdateTextInput(engine);

        // Render scene
        Engine_RenderScene(engine, active_scene);

        // Render UI
        Engine_DrawRetainedUI(engine, active_scene);
        Render_UIRender(engine->renderer, UI_GetContext(), Platform_GetWindowWidth(engine->window), Platform_GetWindowHeight(engine->window));

        // Process destroy queue
        Scene_ProcessDestroyQueue(active_scene);
        
        // Swap Buffers & Reset Input arrays
        Engine_EndFrame(engine);
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

    // Swap the OS window buffers to display the new frame
    Platform_SwapBuffers(engine->window);

    // Cycle the input arrays for the next frame
    Input_Update();
}