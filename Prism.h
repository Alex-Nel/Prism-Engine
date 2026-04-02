#ifndef PRISMENGINE_H
#define PRISMENGINE_H

#include <stdbool.h>
#include <stdint.h>

// Include the modules the user will actually need to write their game
#include "core/core.h"
#include "render/render.h"
#include "scene/scene.h"
#include "platform/platform.h"
#include "assets/asset_manager.h"

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


// ----- Mouse related functions ---- //
// Sets the Relative Mouse mode
void Engine_CaptureMouse();
void Engine_ReleaseMouse();
bool Engine_IsMouseCaptured();


// ----- FPS related functions ---- //
void Engine_SetTargetFPS(uint32_t fps);
uint32_t Engine_GetTargetFPS();


#endif // ENGINE_H