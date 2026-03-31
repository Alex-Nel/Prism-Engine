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

// The Front Bookend: Ticks time, polls OS events, routes structural events, feeds Input
bool Engine_IsRunning();

// Main render function for a scene
void Engine_RenderScene(Scene* scene);

// The Back Bookend: Executes render queue, swaps buffers, cycles Input state
void Engine_EndFrame();

// Shuts down all systems safely
void Engine_Shutdown();




// Sets the Relative Mouse mode
void Engine_CaptureMouse();
void Engine_ReleaseMouse();

// Returns if the mouse is captured
bool Engine_IsMouseCaptured();

#endif // ENGINE_H