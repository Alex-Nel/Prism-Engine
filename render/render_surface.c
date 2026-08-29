#include "render_surface.h"
#include "../platform/platform_core.h"





// Configures the platform surface based on the API
void Render_ConfigurePlatformSurface(GraphicsAPI api)
{
    switch (api)
    {
        case GRAPHICS_API_OPENGL:
            Platform_SetGLAttribute(GRAPHICS_GL_ATTR_CONTEXT_MAJOR_VERSION, 3);
            Platform_SetGLAttribute(GRAPHICS_GL_ATTR_CONTEXT_MINOR_VERSION, 3);
            Platform_SetGLAttribute(GRAPHICS_GL_ATTR_CONTEXT_PROFILE_MASK, GRAPHICS_GL_PROFILE_CORE);
            Platform_SetGLAttribute(GRAPHICS_GL_ATTR_DOUBLEBUFFER, 1);
            Platform_SetGLAttribute(GRAPHICS_GL_ATTR_DEPTH_SIZE, 24);
            break;
        case GRAPHICS_API_VULKAN:
        case GRAPHICS_API_DIRECTX:
        case GRAPHICS_API_SOFTWARE:
        case GRAPHICS_API_NONE:
        default:
            break;
    }
}