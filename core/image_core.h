#ifndef CORE_IMAGE_H
#define CORE_IMAGE_H

#include <stdbool.h>




// The CPU container for raw pixel data
typedef struct ImageData
{
    unsigned char* pixels;
    int width;
    int height;
    int channels; // 3 for RGB (JPG), 4 for RGBA (PNG)
} ImageData;



// Loads an image from disk into CPU RAM
ImageData Image_Load(const char* filepath, bool inverted);

// Loads an image from a raw memory buffer (for embedded textures)
ImageData Image_LoadFromMemory(const unsigned char* buffer, int length, bool inverted);

// Rotates an image 90 degrees counter clock wise
void Image_Rotate90CW(ImageData* img);

// Frees the CPU RAM
void Image_Free(ImageData* data);





// The CPU container for float (HDR) pixel data
typedef struct ImageDataFloat
{
    float* pixels;
    int width;
    int height;
    int channels; 
} ImageDataFloat;



// Loads an HDR image from disk into CPU RAM
ImageDataFloat Image_LoadFloat(const char* filepath, bool inverted);

// Frees the CPU RAM of an HDR image
void Image_FreeFloat(ImageDataFloat* data);





#endif