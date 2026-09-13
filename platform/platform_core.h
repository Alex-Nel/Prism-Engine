#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include "../core/event_core.h"
#include "../core/input_core.h"
#include "../core/log_core.h"
#include "../core/time_core.h"
#include "../core/graphics_core.h"


// Definition of a "window", implemented by each platform
typedef struct Window Window;

// Opaque mutex primitives
typedef struct PlatformMutex PlatformMutex;

// Opaque condition primitive
typedef struct PlatformCondition PlatformCondition;

// Opaque thread primitive
typedef struct PlatformThread PlatformThread;

// Entry point for a platform thread. Return value is reported by JoinThread.
typedef int (*PlatformThreadFunction)(void* user_data);

// Defines a per-window callback for events processed inside a native modal loop
typedef void (*PlatformEventWatchCallback)(Window* window, void* user_data);





// ----- Functions for initializing and shuting down platform -----

// Sets a graphics surface hint before Platform_Init (OpenGL only).
void Platform_SetGLAttribute(GraphicsGLAttribute attr, int value);

// Initializes a window with a title, width, height, and graphics API.
Window* Platform_Init(const char* title, uint32_t width, uint32_t height, GraphicsAPI api);

// Shuts down the window
void Platform_Shutdown(Window* window);

// Returns the native window handle
void* Platform_GetNativeWindow(Window* window);

// Returns the platform-independent identifier assigned to a window
uint32_t Platform_GetWindowID(Window* window);

// Gets the x and y position of the window (from the top left)
void Platform_GetWindowPosition(Window* window, int* x, int* y);

// Returns the width of a window
uint32_t Platform_GetWindowWidth(Window* window);

// Returns the height of a window
uint32_t Platform_GetWindowHeight(Window* window);

// Sets the width and height of a window
void Platform_SetWindowSize(Window* window, uint32_t width, uint32_t height);

// Sets the window to minimized or shown
void Platform_SetWindowMinimized(Window* window, bool minimized);

// Returns whether the window is currently minimized
bool Platform_IsWindowMinimized(Window* window);





// ----- Platform utility functions -----

// Registers a modal event callback for one window
void Platform_SetEventWatchCallback(Window* window, PlatformEventWatchCallback callback, void* user_data);

// Platform specific function to poll events
bool Platform_PollEvents(Event* e);

// Get the time since the platform has existed
double Platform_GetTime();

// Delays a platform window for a specified ms
void Platform_Delay(uint32_t ms);

// Raises the window to the foreground
bool Platform_RaiseWindow(Window* window);

// Returns whether the mouse is captured
bool Platform_IsMouseCaptured(Window* window);

// Enabled/Disables relative mouse mode
void Platform_SetRelativeMouseMode(Window* window, bool enabled);

// Warps the mouse to the middle of the screen
void Platform_WarpMouseToMiddle(Window* window);

// Starts accepting text input from a window
void Platform_StartTextInput(Window* window);

// Stops accepting text input from a window
void Platform_StopTextInput(Window* window);

// Returns whether a window is currently accepting input
bool Platform_IsTextInputActive(Window* window);

// Copies UTF-8 text to the system clipboard
bool Platform_SetClipboardText(const char* text);

// Returns allocated UTF-8 text from the system clipboard.
char* Platform_GetClipboardText();

// Releases text returned by Platform_GetClipboardText
void Platform_FreeClipboardText(char* text);





// ----- OpenGL specific platform functions -----

// Makes an OpenGL context from the platform
void* Platform_GL_CreateContext(void* native_window);

// Destroys an OpenGL context from the platform
void Platform_GL_DestroyContext(void* context);

// Makes the given opengl context the current one displaed to a window
bool Platform_GL_MakeCurrent(void* native_window, void* context);

// Released the current opengl context from the platform
void Platform_GL_ReleaseCurrent(void);

// Swaps opengl buffers on a platform window
void Platform_GL_SwapBuffers(void* native_window);

// Sets the opengl swap interval
void Platform_GL_SetSwapInterval(int interval);

// returns the opengl process address from the platform
void* Platform_GL_GetProcAddress(const char* name);





// ----- Synchronization -----

// Creates a mutex from the platform
PlatformMutex* Platform_CreateMutex();

// Destroys a mutex
void Platform_DestroyMutex(PlatformMutex* mutex);

// Locks a mutex
void Platform_LockMutex(PlatformMutex* mutex);

// Unlocks a mutex
void Platform_UnlockMutex(PlatformMutex* mutex);


// Creates a condition from the platform
PlatformCondition* Platform_CreateCondition();

// Destroys a condition
void Platform_DestroyCondition(PlatformCondition* condition);

// Signals a condition
void Platform_SignalCondition(PlatformCondition* condition);

// Broadcasts a condition
void Platform_BroadcastCondition(PlatformCondition* condition);

// Waits until a condition is met
void Platform_WaitCondition(PlatformCondition* condition, PlatformMutex* mutex);

// Waits until a condition is met or a certain time has passed
bool Platform_WaitConditionTimeout(PlatformCondition* condition, PlatformMutex* mutex, uint32_t timeout_ms);





// ----- Threads -----

// Creates a joinable platform thread
PlatformThread* Platform_CreateThread(PlatformThreadFunction function, const char* name, void* user_data);

// Waits for a platform thread to finish and releases its SDL thread object (optionally returns a result)
void Platform_JoinThread(PlatformThread* thread, int* result);

// Returns a stable identifier for the calling thread
uint64_t Platform_GetCurrentThreadID();





#endif // PLATFORM_H