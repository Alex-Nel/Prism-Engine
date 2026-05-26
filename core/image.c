#include <stdbool.h>
#include "image.h"
#include "log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"



// Loads image from file path
ImageData Image_Load(const char* filepath)
{
    ImageData data = {0};
    
    // OpenGL expects the Y=0 coordinate to be at the bottom of the image.
    // Most image formats put Y=0 at the top. This flips it so textures aren't upside down
    stbi_set_flip_vertically_on_load(1); 
    
    // stbi_load automatically allocates the memory and decodes the image
    data.pixels = stbi_load(filepath, &data.width, &data.height, &data.channels, 0);

    return data;
}



// Loads image from within memory
ImageData Image_LoadFromMemory(const unsigned char* buffer, int length)
{
    ImageData img = {0};
    
    // Keep the image right-side up for OpenGL
    stbi_set_flip_vertically_on_load(true); 
    
    // Read directly from the RAM buffer instead of the hard drive
    img.pixels = stbi_load_from_memory(buffer, length, &img.width, &img.height, &img.channels, 4);
    
    if (img.pixels)
        img.channels = 4; // Force 4 channels (RGBA)
    else
        Log_Error("Failed to load embedded image from memory!");
    
    return img;
}



// Free's the image data from memory
void Image_Free(ImageData* data)
{
    if (data && data->pixels)
    {
        stbi_image_free(data->pixels);
        data->pixels = NULL;
    }
}