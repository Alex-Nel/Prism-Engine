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
        static void SetTargetFPS(uint32_t target_fps);
        static uint32_t GetTargetFPS();
    };
}