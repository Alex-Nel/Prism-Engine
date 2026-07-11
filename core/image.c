#include <stdbool.h>
#include "image.h"
#include "log.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"



// Loads image from file path
ImageData Image_Load(const char* filepath, bool inverted)
{
    ImageData data = {0};
    
    // OpenGL expects the Y=0 coordinate to be at the bottom of the image.
    // Most image formats put Y=0 at the top. This flips it so textures aren't upside down
    stbi_set_flip_vertically_on_load(inverted); 
    
    // stbi_load automatically allocates the memory and decodes the image. Force 4 channels (RGBA) for 100% memory alignment safety with OpenGL.
    data.pixels = stbi_load(filepath, &data.width, &data.height, &data.channels, 4);
    if (data.pixels)
        data.channels = 4;

    return data;
}



// Loads image from within memory
ImageData Image_LoadFromMemory(const unsigned char* buffer, int length, bool inverted)
{
    ImageData img = {0};
    
    // Keep the image right-side up for OpenGL
    stbi_set_flip_vertically_on_load(inverted); 
    
    // Read directly from the RAM buffer instead of the hard drive
    img.pixels = stbi_load_from_memory(buffer, length, &img.width, &img.height, &img.channels, 4);
    
    if (img.pixels)
        img.channels = 4; // Force 4 channels (RGBA)
    else
        Log_Error("Failed to load embedded image from memory!");
    
    return img;
}



// Rotates an image's raw pixel array 90 degrees clockwise
void Image_Rotate90CW(ImageData* img)
{
    if (!img || !img->pixels) return;

    int w = img->width;
    int h = img->height;
    int c = img->channels;

    // Allocate a new buffer for the rotated pixels
    unsigned char* rotated_pixels = (unsigned char*)malloc(w * h * c);

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            // Calculate the new X and Y for a 90-degree clockwise rotation
            int new_x = h - 1 - y;
            int new_y = x;

            // Calculate 1D array indices
            int old_index = (y * w + x) * c;
            int new_index = (new_y * h + new_x) * c;

            // Copy the pixel data (handles 3 or 4 channels automatically)
            for (int i = 0; i < c; i++) {
                rotated_pixels[new_index + i] = img->pixels[old_index + i];
            }
        }
    }

    // Free the old array and assign the new one!
    free(img->pixels);
    img->pixels = rotated_pixels;
    
    // Swap the width and height
    img->width = h;
    img->height = w;
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