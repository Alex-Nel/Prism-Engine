#ifndef PRISMENGINE_H
#define PRISMENGINE_H

#include <stdbool.h>
#include <stdint.h>

// Include all modules
#include "core/core.h"
#include "audio/audio.h"
#include "render/render.h"
#include "scene/scene.h"
#include "platform/platform_core.h"
#include "assets/asset_manager.h"


// Struct for an "engine"
typedef struct PrismEngine
{
    Window* window;
    Renderer* renderer;
    bool is_running;
    uint32_t target_fps;
} PrismEngine;

typedef void (*EngineUpdateCallback)(void);





// --- Structures for frustum culling ---

typedef struct FrustumPlane
{
    Vector3 normal;
    float distance;
} FrustumPlane;

typedef struct Frustum
{
    FrustumPlane planes[6];
} Frustum;




// Extracts the 6 planes from a view-projection matrix
Frustum Frustum_ExtractFromMatrix(Matrix4 vp);

// Checks if a sphere is inside the frustum
bool Frustum_ContainsAABB(Frustum* frustum, AABB local_aabb, Matrix4 world_matrix);





// Initializes Platform, Core, and Render systems
bool Engine_Init(const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api);

// Get the main window pointer
Window* Engine_GetMainWindow();

// Allows API to set custom runtime logic
void Engine_SetPreUpdateCallback(EngineUpdateCallback callback);


void Engine_Run(Scene* active_scene);
bool Engine_IsRunning();               // Deprecated
void Engine_SetClearColor(float r, float g, float b, float a);
void Engine_RenderScene(Scene* scene); // Main render function for a scene
void Engine_EndFrame();                // The Back Bookend: Executes render queue, swaps buffers, cycles Input state

// Shuts down all systems
void Engine_Shutdown();


// Captures the mouse to the window
void Engine_CaptureMouse();
// Releases the mouse to the OS
void Engine_ReleaseMouse();
// Returns if the mouse is currently captured by the engine
bool Engine_IsMouseCaptured();


// Set the target FPS of the engine
void Engine_SetTargetFPS(uint32_t fps);
// Returns the current target FPS
uint32_t Engine_GetTargetFPS();


#endif