#include "../../include/core/Time.hpp"


extern "C"
{
    #include "../../../core/timeCore.h"
}


namespace Prism
{
    float Time::DeltaTime() {
        return ::Time_DeltaTime();
    }
    
    void Time::SetTargetFPS(uint32_t target_fps) {
        ::Time_SetTargetFPS(target_fps);
    }
    
    uint32_t Time::GetTargetFPS() {
        return ::Time_GetTargetFPS();
    }
}