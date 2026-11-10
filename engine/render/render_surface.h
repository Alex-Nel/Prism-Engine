#ifndef RENDER_SURFACE_H
#define RENDER_SURFACE_H



#include "../core/graphics_core.h"
#include <stdbool.h>



// Applies API-specific surface hints before Platform_Init creates the window.
void Render_ConfigurePlatformSurface(GraphicsAPI api);



// Renderer-owned OpenGL context + window
typedef struct RenderGLSurface
{
    void* native_window;
    void* gl_context;
} RenderGLSurface;





// Initializes the OpenGL surface using the platform
bool RenderGLSurface_Init(RenderGLSurface* surface, void* native_window);

// Shuts down the OpenGL context
void RenderGLSurface_Shutdown(RenderGLSurface* surface);

// Makes a specific Opengl surface the current for the window
bool RenderGLSurface_MakeCurrent(RenderGLSurface* surface);

// Releases the current openGL context
void RenderGLSurface_ReleaseCurrent(RenderGLSurface* surface);

// Presents the OpenGL surface to the current window
void RenderGLSurface_Present(RenderGLSurface* surface);

// Enabled/Disables VSync for an OpenGL surface
void RenderGLSurface_SetVSync(RenderGLSurface* surface, bool enabled);

// Gets the proc address of the OpenGL surface
void* RenderGLSurface_GetProcAddress(const char* name);





#endif // RENDER_SURFACE_H