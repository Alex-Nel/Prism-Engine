#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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



// Free's the image data from memory
void Image_Free(ImageData* data)
{
    if (data && data->pixels)
    {
        stbi_image_free(data->pixels);
        data->pixels = NULL;
    }
}