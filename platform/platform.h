#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "../core/event.h"
#include "../core/input.h"
#include "../core/timeCore.h"


// Enum for different graphics API's
typedef enum GraphicsAPI
{
    GRAPHICS_API_OPENGL,
    GRAPHICS_API_VULKAN,
    GRAPHICS_API_DIRECTX,
    GRAPHICS_API_NONE     // Useful for headless dedicated servers
} GraphicsAPI;


// Definition of window, implemented by each platform
typedef struct Window Window;


// Functions for initializing and shuting down platform
Window* Platform_Init(const char* title, uint32_t width, uint32_t height, GraphicsAPI api);
void Platform_Shutdown(Window* window);



// Gets the OS-specific graphics function pointer (hides SDL_GL_GetProcAddress)
void* Platform_GetProcAddress(const char* name);

// Platform specific function to poll events
bool Platform_PollEvents(Event* e);

// Swap buffers (for double-buffered rendering)
void Platform_SwapBuffers(Window* window);

// Get the time since the platform has existed
double Platform_GetTime(void);

// Delays a platform window for a specified ms
void Platform_Delay(uint32_t ms);

// Enabled/Disables relative mouse mode
void Platform_SetRelativeMouseMode(Window* window, bool enabled);

// Warps the mouse to the middle of the screen
void Platform_WarpMouse(Window* window);



#endif