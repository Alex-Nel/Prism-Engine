#pragma once
#include <cstdint>
#include "../PrismAPI.hpp"

namespace Prism
{
    // --- Contains time related functions (DeltaTime, TargetFPS)
    class PRISM_API Time
    {
    public:
        Time() = delete;

        static float DeltaTime();
        static float FixedDeltaTime();
        static float UnscaledDeltaTime();
        static void SetFixedDeltaTime(float fixed_dt);
        static void SetTimeScale(float scale);
        static float GetTimeScale();
        static double TimeElapsed();
        static double UnscaledTimeElapsed();
        static uint64_t GetFrameCount();
    };
}