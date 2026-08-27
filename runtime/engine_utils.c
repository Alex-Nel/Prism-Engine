#include "engine_runtime.h"



// Get a pointer to the main window
Window* Engine_GetMainWindow(PrismEngine* engine)
{
    return engine->window;
}





// Get a pointer to the main renderer
Renderer* Engine_GetRenderer(PrismEngine* engine)
{
    return engine->renderer;
}





// Captures the mouse to the window
void Engine_CaptureMouse(PrismEngine* engine)
{
    Platform_SetRelativeMouseMode(engine->window, true);
}





// Releases the mouse from the window
void Engine_ReleaseMouse(PrismEngine* engine)
{
    Platform_SetRelativeMouseMode(engine->window, false);
    Platform_WarpMouseToMiddle(engine->window);
}





// Returns if the mouse is captured
bool Engine_IsMouseCaptured(PrismEngine* engine)
{
    return Platform_IsMouseCaptured(engine->window);
}





// Sets the engines target FPS
void Engine_SetTargetFPS(PrismEngine* engine, uint32_t fps)
{
    engine->target_fps = fps;
    Time_SetTargetFPS(fps);
}


// Returns the current target FPS
uint32_t Engine_GetTargetFPS(PrismEngine* engine)
{
    return Time_GetTargetFPS();
}