#pragma once

extern "C"
{
    #include "core/timeCore.h"
}

namespace Prism
{
    class Time
    {
    public:
        // Delete the constructor so users can't accidentally instantiate 'Time t;'
        Time() = delete;


        // Returns the time elapsed between the current and previous frame in seconds.
        static float DeltaTime() {
            return ::Time_DeltaTime();
        }

        
        // Sets the engine's target framerate. Set to 0 for uncapped.
        static void SetTargetFPS(uint32_t target_fps) {
            ::Time_SetTargetFPS(target_fps);
        }

        // Retrieves the current target framerate constraint.
        static uint32_t GetTargetFPS() {
            return ::Time_GetTargetFPS();
        }
    };
}