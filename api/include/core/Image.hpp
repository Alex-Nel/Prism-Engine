#pragma once
#include <string>

namespace Prism
{
    // --- Image Data Wrapper ---
    struct ImageData
    {    
        void* pixels;
        int width;
        int height;
        int channels;

        // Default empty constructor
        ImageData();

        // --- Memory Management Methods ---
        void Free();

        // --- Static Factory Methods ---
        static ImageData Load(const std::string& filepath);
    };
}