#ifndef CORE_TIME_H
#define CORE_TIME_H

#include <stdint.h>

typedef double (*Time_GetTime)(void);
typedef void (*Time_Delay)(uint32_t ms);


// Initialize the time system and set a target FPS (e.g., 60).
// Set to 0 for an uncapped framerate.
void Time_Init(uint32_t target_fps, Time_GetTime get_time_cb, Time_Delay delay_cb);

// Called once at the VERY START of your main loop.
// Calculates dt and pauses the engine if it's running too fast.
void Time_Tick();

// Returns the time elapsed between the current and previous frame in seconds.
float Time_DeltaTime();


// Functions for getting and setting the target FPS
void Time_SetTargetFPS(uint32_t target_fps);
uint32_t Time_GetTargetFPS();


#endif