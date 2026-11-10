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















// Initializes the OpenGL surface using the platform
bool RenderGLSurface_Init(RenderGLSurface* surface, void* native_window)
{
    if (!surface || !native_window)
        return false;
    
    void* context = Platform_GL_CreateContext(native_window);
    if (!context)
        return false;
    
    if (!Platform_GL_MakeCurrent(native_window, context))
    {
        Platform_GL_DestroyContext(context);
        return false;
    }
    
    surface->native_window = native_window;
    surface->gl_context = context;
    return true;
}










// Shuts down the OpenGL context
void RenderGLSurface_Shutdown(RenderGLSurface* surface)
{
    if (!surface)
        return;

    if (surface->native_window && surface->gl_context)
    {
        Platform_GL_MakeCurrent(surface->native_window, NULL);
        Platform_GL_DestroyContext(surface->gl_context);
    }
    
    surface->native_window = NULL;
    surface->gl_context = NULL;
}










// Makes a specific Opengl surface the current for the window
bool RenderGLSurface_MakeCurrent(RenderGLSurface* surface)
{
    if (!surface || !surface->native_window || !surface->gl_context)
        return false;

    return Platform_GL_MakeCurrent(surface->native_window, surface->gl_context);
}










// Releases the current openGL context
void RenderGLSurface_ReleaseCurrent(RenderGLSurface* surface)
{
    Platform_GL_ReleaseCurrent();
}










// Presents the OpenGL surface to the current window
void RenderGLSurface_Present(RenderGLSurface* surface)
{
    if (!surface || !surface->native_window)
        return;

    Platform_GL_SwapBuffers(surface->native_window);
}










// Enabled/Disables VSync for an OpenGL surface
void RenderGLSurface_SetVSync(RenderGLSurface* surface, bool enabled)
{
    Platform_GL_SetSwapInterval(enabled ? 1 : 0);
}










// Gets the proc address of the OpenGL surface
void* RenderGLSurface_GetProcAddress(const char* name)
{
    return Platform_GL_GetProcAddress(name);
}