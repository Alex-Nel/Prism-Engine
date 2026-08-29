#ifndef RENDER_SURFACE_H
#define RENDER_SURFACE_H
#include "../core/graphics_core.h"\



// Applies API-specific surface hints before Platform_Init creates the window.
void Render_ConfigurePlatformSurface(GraphicsAPI api);



#endif // RENDER_SURFACE_H