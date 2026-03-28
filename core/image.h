#ifndef CORE_IMAGE_H
#define CORE_IMAGE_H



// The CPU container for raw pixel data
typedef struct ImageData
{
    unsigned char* pixels;
    int width;
    int height;
    int channels; // 3 for RGB (JPG), 4 for RGBA (PNG)
} ImageData;



// Loads an image from disk into CPU RAM
ImageData Image_Load(const char* filepath);

// Frees the CPU RAM
void Image_Free(ImageData* data);



#endif