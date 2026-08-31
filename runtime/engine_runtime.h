#ifndef ENGINE_RUNTIME_H
#define ENGINE_RUNTIME_H


#include "Prism.h"
#include "render/render_frame.h"
#include <stdint.h>



typedef void (*EngineUpdateCallback)(void);
typedef void (*EngineModalCallback)(void*);

// Struct for an "engine"
typedef struct PrismEngine
{
    // Key pointers
    Window* window;
    Renderer* renderer;
    Scene* active_scene;

    // Engine state variables
    bool is_running;
    bool is_simulating;
    float accumulator;
    uint32_t target_fps;
    
    // Modal and update callbacks
    EngineUpdateCallback pre_update_callback;
    EngineModalCallback modal_callback;
    void* modal_userdata;
    
    // Render Queue
    RenderFrameQueue frame_queue;
    uint64_t render_frame_counter;

    // Written by the main thread on resize events; consumed before GPU draw.
    uint32_t pending_frame_width;
    uint32_t pending_frame_height;
    bool pending_framebuffer_resize;
} PrismEngine;



// Struct to sort cameras before rendering
typedef struct ActiveCamera
{
    uint32_t entity_id;
    int render_order;
} ActiveCamera;





// ----- Core functions -----

// Initializes Platform, Core, and Render systems
bool Engine_Init(PrismEngine* engine, const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api);

// Shuts down all systems
void Engine_Shutdown(PrismEngine* engine);

// Allows API to set custom runtime logic
void Engine_SetPreUpdateCallback(PrismEngine* engine, EngineUpdateCallback callback);

// Sets a custom function for the modal loop
void Engine_SetModalCallback(PrismEngine* engine, EngineModalCallback callback, void* userdata);

// Toggles physics and script updates
void Engine_SetSimulationMode(PrismEngine* engine, bool is_simulating);

// Updates the engines state
void Engine_Update(PrismEngine* engine, Scene* active_scene);

// Renders everything in a scene including overlays and UI
void Engine_Render(PrismEngine* engine, Scene* active_scene);

// Starts the main engine loop
void Engine_Run(PrismEngine* engine, Scene* active_scene);
bool Engine_IsRunning(PrismEngine* engine); // Deprecated

// Swaps buffers, cycles Input state
void Engine_EndFrame(PrismEngine* engine);





// ----- Rendering functions -----

// Gathers all the lights in a scene and puts them into a lighting packet.
void Engine_GatherSceneLights(PrismEngine* engine, Scene* scene, RenderLighting* lighting, DirectionalLightData* dir_lights, uint32_t max_dir, PointLightData* point_lights, uint32_t max_point, SpotLightData* spot_lights, uint32_t max_spot);

// Gathers all the reflection probes in a scene and puts them into a lighting packet.
void Engine_GatherReflectionProbes(PrismEngine* engine, Scene* scene, RenderLighting* lighting, ReflectionProbeData* probes, uint32_t max_probes);

// Applies capture results from a completed render frame back into the scene.
void Engine_ApplyFrameResults(PrismEngine* engine, Scene* scene, const RenderFrame* frame);

// Applies capture results from the last DrawWorld back into the scene.
void Engine_ApplyReflectionProbeResults(PrismEngine* engine, Scene* scene);

// Gathers all the cameras in a scene and sorts them
uint32_t Engine_GatherAndSortCameras(PrismEngine* engine, Scene* scene, ActiveCamera* active_cameras);

// Extracts scene geometry into a RenderItem array for DrawWorld. Spatial culling is done by the renderer.
uint32_t Engine_GatherVisibleGeometry(Scene* scene, Vector3 cam_pos, uint32_t culling_masks, RenderItem* out, uint32_t max, RenderFrame* frame_for_bones);

// Builds an immutable render snapshot from the current scene state.
void Engine_BuildRenderFrame(PrismEngine* engine, Scene* scene, RenderFrame* frame);

// Main function to render a scene
void Engine_RenderScene(PrismEngine* engine, Scene* scene);

// Records a pending framebuffer resize (main thread only)
void Engine_NotifyFramebufferResize(PrismEngine* engine, uint32_t width, uint32_t height);

// Applies any pending framebuffer resize on the render path (before DrawFrame)
void Engine_ApplyPendingFramebufferResize(PrismEngine* engine);





// ----- Utility functions -----

// Get the main window pointer
Window* Engine_GetMainWindow(PrismEngine* engine);

// Get the main renderer pointer
Renderer* Engine_GetRenderer(PrismEngine* engine);

// Captures the mouse to the window
void Engine_CaptureMouse(PrismEngine* engine);

// Releases the mouse to the OS
void Engine_ReleaseMouse(PrismEngine* engine);

// Returns if the mouse is currently captured by the engine
bool Engine_IsMouseCaptured(PrismEngine* engine);

// Set the target FPS of the engine
void Engine_SetTargetFPS(PrismEngine* engine, uint32_t fps);

// Returns the current target FPS
uint32_t Engine_GetTargetFPS(PrismEngine* engine);





#endif // ENGINE_RUNTIME_H