#ifndef CORE_TIME_H
#define CORE_TIME_H

#include <stdint.h>

typedef double (*Time_GetTime)(void);
typedef void (*Time_Delay)(uint32_t ms);



// --- Time Module Lifecycle ---

// Initialize the time system and set a target FPS.
// Set to 0 for an uncapped framerate.
void Time_Init(uint32_t target_fps, Time_GetTime get_time_pf, Time_Delay delay_pf);

// Called once at the start of a main loop.
// Calculates dt and pauses the engine if it's running too fast.
void Time_Tick();



// --- Functions for getting time values at runtime ---

// Returns the time elapsed between the current and previous frame in seconds.
float Time_DeltaTime();

// Returns the interval of time that fixed frame updates happen
float Time_FixedDeltaTime();

// Returns the real elapsed time between frames, ignoring the time scale.
float Time_UnscaledDeltaTime();

// Returns the current time scale
float Time_GetTimeScale();



// --- Functions for getting and setting the target FPS ---

void Time_SetTargetFPS(uint32_t target_fps);
uint32_t Time_GetTargetFPS();



// --- Functions for setting the time scale and delta time ---

void Time_SetTimeScale(float scale);
void Time_SetFixedDeltaTime(float fixed_dt);



// --- Functions for getting precise time trackers ---

double Time_TimeElapsed();
double Time_UnscaledTimeElapsed();
uint64_t Time_GetFrameCount();



#endif