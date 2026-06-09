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

    float Time::FixedDeltaTime() {
        return ::Time_FixedDeltaTime();
    }

    float Time::UnscaledDeltaTime() {
        return ::Time_UnscaledDeltaTime();
    }

    void Time::SetFixedDeltaTime(float fixed_dt){
        ::Time_SetFixedDeltaTime(fixed_dt);
    }

    void Time::SetTimeScale(float scale) {
        ::Time_SetTimeScale(scale);
    }

    float Time::GetTimeScale() {
        return ::Time_GetTimeScale();
    }

    double Time::TimeElapsed() {
        return ::Time_TimeElapsed();
    }

    double Time::UnscaledTimeElapsed() {
        return ::Time_UnscaledTimeElapsed();
    }

    uint64_t Time::GetFrameCount() {
        return ::Time_GetFrameCount();
    }
}