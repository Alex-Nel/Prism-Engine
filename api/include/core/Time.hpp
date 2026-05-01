#pragma once
#include <cstdint>

namespace Prism
{
    // --- Contains time related functions (DeltaTime, TargetFPS)
    class Time
    {
    public:
        Time() = delete;

        static float DeltaTime();
        static void SetTargetFPS(uint32_t target_fps);
        static uint32_t GetTargetFPS();
    };
}