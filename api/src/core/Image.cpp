#include "../../include/core/Image.hpp"


extern "C"
{
    #include "../../../core/image.h"
}


namespace Prism
{
    Image::Image() : pixels(nullptr), width(0), height(0), channels(0) {}

    void Image::Free() 
    {
        // Reconstruct the C struct to pass it to the backend
        ::ImageData raw = { static_cast<unsigned char*>(pixels), width, height, channels };
        ::Image_Free(&raw);
        pixels = nullptr; // Safety null
    }

    void Image::Image_Rotate90CW()
    {
        ::Image_Rotate90CW(static_cast<::ImageData*>(this->pixels));
    }

    Image Image::Load(const std::string& filepath, bool inverted) 
    {
        ::ImageData raw = ::Image_Load(filepath.c_str(), inverted);
        
        // Map the C struct back to a C++ struct
        Image img;
        img.pixels = raw.pixels;
        img.width = raw.width;
        img.height = raw.height;
        img.channels = raw.channels;
        
        return img;
    }
}