#pragma once
#include <string>
#include "../PrismAPI.hpp"

namespace Prism
{
    // --- Image Data Wrapper ---
    struct PRISM_API Image
    {    
        void* pixels;
        int width;
        int height;
        int channels;

        // Default empty constructor
        Image();

        // --- Memory Management Methods ---
        void Free();
        void Image_Rotate90CW();

        // --- Static Factory Methods ---
        static Image Load(const std::string& filepath, bool inverted = true);
    };
}