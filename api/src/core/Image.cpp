#include "../../include/core/Image.hpp"


extern "C"
{
    #include "../../../core/image.h"
}


namespace Prism
{
    ImageData::ImageData() : pixels(nullptr), width(0), height(0), channels(0) {}

    void ImageData::Free() 
    {
        // Reconstruct the C struct to pass it to the backend
        ::ImageData raw = { static_cast<unsigned char*>(pixels), width, height, channels };
        ::Image_Free(&raw);
        pixels = nullptr; // Safety null
    }

    ImageData ImageData::Load(const std::string& filepath) 
    {
        ::ImageData raw = ::Image_Load(filepath.c_str());
        
        // Map the C struct back to our safe C++ struct
        ImageData img;
        img.pixels = raw.pixels;
        img.width = raw.width;
        img.height = raw.height;
        img.channels = raw.channels;
        
        return img;
    }
}