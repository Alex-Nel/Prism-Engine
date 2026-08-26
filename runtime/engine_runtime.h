#ifndef ENGINE_RUNTIME_H
#define ENGINE_RUNTIME_H


#include "Prism.h"
#include <stdint.h>



typedef void (*EngineUpdateCallback)(void);
typedef void (*EngineModalCallback)(void*);

// Struct for an "engine"
typedef struct PrismEngine
{
    Window* window;
    Renderer* renderer;
    Scene* active_scene;
    bool is_running;
    bool is_simulating;
    float accumulator;
    uint32_t target_fps;
    EngineUpdateCallback pre_update_callback;
    EngineModalCallback modal_callback;
    void* modal_userdata;
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
void Engine_GatherSceneLights(PrismEngine* engine, Scene* scene, RenderLighting* lighting, DirectionalLightData* dir_lights, PointLightData* point_lights, SpotLightData* spot_lights);

// Gathers all the reflection probes in a scene and puts them into a lighting packet.
void Engine_GatherReflectionProbes(PrismEngine* engine, Scene* scene, RenderLighting* lighting, ReflectionProbeData* probes, uint32_t max_probes);

// Applies all changes to reflection probes back into the scene.
void Engine_ApplyReflectionProbeResults(PrismEngine* engine, Scene* scene, const ReflectionProbeData* probes, uint32_t probe_count);

// Gathers all the cameras in a scene and sorts them
uint32_t Engine_GatherAndSortCameras(PrismEngine* engine, Scene* scene, ActiveCamera* active_cameras);

// Extracts scene geometry into a RenderItem array for DrawWorld. Spatial culling is done by the renderer.
uint32_t Engine_GatherVisibleGeometry(Scene* scene, Vector3 cam_pos, uint32_t culling_masks, RenderItem* out, uint32_t max);

// Main function to render a scene
void Engine_RenderScene(PrismEngine* engine, Scene* scene);





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

// Sets the clear color of an engines renderer
void Engine_SetClearColor(PrismEngine* engine, float r, float g, float b, float a);





#endif // ENGINE_RUNTIME_H