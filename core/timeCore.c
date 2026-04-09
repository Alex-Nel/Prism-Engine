#include "timeCore.h"
#include <stddef.h>


// Platform functions for delays and getting time
static Time_GetTime get_time_func = NULL;
static Time_Delay delay_func = NULL;


// Internal variables for relevant times
static uint32_t current_target_fps = 0;
static double last_time = 0.0;
static float delta_time = 0.0f;
static double target_frame_time = 0.0;



// Initialize time system with a target FPS and the platforms functions
void Time_Init(uint32_t target_fps, Time_GetTime get_time_pf, Time_Delay delay_pf)
{
    current_target_fps = target_fps;
    get_time_func = get_time_pf;
    delay_func = delay_pf;

    // Get the first time if the function exists, otherwise 0
    if (get_time_func)
        last_time = get_time_func();
    else
        last_time = 0.0;

    delta_time = 0.0f;
    
    // Set target frame time depending on target FPS, or 0 if uncapped
    if (target_fps > 0)
    {
        target_frame_time = 1.0 / (double)target_fps;
    }
    else
    {
        target_frame_time = 0.0;
    }
}





void Time_Tick()
{
    if (!get_time_func) return;


    double current_time = get_time_func();
    double elapsed = current_time - last_time;

    // --- Framerate Capping (Software Limiter) ---
    if (target_frame_time > 0.0 && elapsed < target_frame_time)
    {
        // Calculate how much time we have left to pass
        double sleep_time_s = target_frame_time - elapsed;
        uint32_t sleep_time_ms = (uint32_t)(sleep_time_s * 1000.0);

        // Yield CPU to the OS
        if (sleep_time_ms > 0)
        {
            delay_func(sleep_time_ms);
        }

        // Recalculate time after waking up from sleep
        current_time = get_time_func();
        elapsed = current_time - last_time;
    }

    // --- Spiral of Death Prevention ---
    // Cap delta time at 0.1 seconds (10 FPS) to prevent physics explosions
    if (elapsed > 0.1)
    {
        elapsed = 0.1;
    }

    // Update global state
    delta_time = (float)elapsed;
    last_time = current_time;
}





float Time_DeltaTime()
{
    return delta_time;
}





void Time_SetTargetFPS(uint32_t target_fps)
{
    current_target_fps = target_fps;
    
    if (target_fps > 0)
        target_frame_time = 1.0 / (double)target_fps;
    else
        target_frame_time = 0.0;   // 0 means Unlimited FPS
}





uint32_t Time_GetTargetFPS()
{
    return current_target_fps;
}