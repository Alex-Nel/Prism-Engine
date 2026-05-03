#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "../core/event.h"
#include "../core/input.h"
#include "../core/log.h"
#include "../core/timeCore.h"
#include "../render/render.h"


// Definition of a "window", implemented by each platform
typedef struct Window Window;



// Functions for initializing and shuting down platform

// Initializes a window with a title, width, height, and specified graphics API
Window* Platform_Init(const char* title, uint32_t width, uint32_t height, GraphicsAPI api);

// Shuts down the window
void Platform_Shutdown(Window* window);

// Returns the width of a window
uint32_t GetWindowWidth(Window* window);

// Returns the height of a window
uint32_t GetWindowHeight(Window* window);

// Returns the height of a window
void SetWindowSize(Window* window, uint32_t width, uint32_t height);




// Gets the OS-specific graphics function pointer (for openGL)
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



#endif // PLATFORM_H