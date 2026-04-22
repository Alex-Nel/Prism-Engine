#pragma once

#include <string>

extern "C"
{
    #include "image.h"
}

namespace Prism
{
    // ==========================================
    // Image Data Wrapper
    // ==========================================
    struct ImageData : public ::ImageData
    {    
        // Default empty constructor
        ImageData() {
            pixels = nullptr;
            width = 0;
            height = 0;
            channels = 0;
        }

        // Implicit conversions from C struct
        ImageData(const ::ImageData& raw) {
            pixels = raw.pixels;
            width = raw.width;
            height = raw.height;
            channels = raw.channels;
        }

        operator ::ImageData() const { return *this; }



        // --- Memory Management Methods ---
        
        // Frees the memory allocated by stb_image
        void Free() {
            ::Image_Free(this);
        }



        // --- Static Factory Methods ---
        
        // Loads an image from disk
        static ImageData Load(const std::string& filepath) {
            // Converts the std::string back to a const char* for the C backend
            return ::Image_Load(filepath.c_str());
        }
    };
}