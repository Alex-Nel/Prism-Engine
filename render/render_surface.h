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



bool RenderGLSurface_Init(RenderGLSurface* surface, void* native_window);

void RenderGLSurface_Shutdown(RenderGLSurface* surface);

bool RenderGLSurface_MakeCurrent(RenderGLSurface* surface);

void RenderGLSurface_ReleaseCurrent(RenderGLSurface* surface);

void RenderGLSurface_Present(RenderGLSurface* surface);

void RenderGLSurface_SetVSync(RenderGLSurface* surface, bool enabled);

void* RenderGLSurface_GetProcAddress(const char* name);



#endif // RENDER_SURFACE_H