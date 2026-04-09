#ifndef PRISMENGINE_H
#define PRISMENGINE_H

#include <stdbool.h>
#include <stdint.h>

// Include all modules
#include "core/core.h"
#include "render/render.h"
#include "scene/scene.h"
#include "platform/platform.h"
#include "assets/asset_manager.h"


// Struct for an "engine"
typedef struct PrismEngine
{
    const char* window_title;
    uint32_t window_width;
    uint32_t window_height;
    uint32_t target_fps;
} PrismEngine;


// Initializes Platform, Core, and Render systems
bool Engine_Init(const char* window_title, uint32_t window_width, uint32_t window_height, uint32_t target_fps, GraphicsAPI api);


bool Engine_IsRunning();               // The Front Bookend: Ticks time, polls OS events, routes structural events, feeds Input
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