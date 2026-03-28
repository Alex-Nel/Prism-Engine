#include "../platform.h"
#include "../../include/SDL3/SDL.h"
#include <stdio.h>
#include <stdlib.h>



// Internal state
// static SDL_Window* window = NULL;
// static SDL_GLContext gl_context = NULL;

struct Window
{
    SDL_Window* sdl_window;
    SDL_GLContext gl_context;
    float width;
    float height;
    GraphicsAPI current_api;
    bool should_close;
};



// --- Helper: Translate SDL Keys to Engine Keys ---
static KeyCode TranslateKey(SDL_Keycode sdl_key)
{
    switch (sdl_key)
    {
        case SDLK_A: return KEYCODE_A;
        case SDLK_B: return KEYCODE_B;
        case SDLK_C: return KEYCODE_C;
        case SDLK_D: return KEYCODE_D;
        case SDLK_E: return KEYCODE_E;
        case SDLK_F: return KEYCODE_F;
        case SDLK_G: return KEYCODE_G;
        case SDLK_H: return KEYCODE_H;
        case SDLK_I: return KEYCODE_I;
        case SDLK_J: return KEYCODE_J;
        case SDLK_K: return KEYCODE_K;
        case SDLK_L: return KEYCODE_L;
        case SDLK_M: return KEYCODE_M;
        case SDLK_N: return KEYCODE_N;
        case SDLK_O: return KEYCODE_O;
        case SDLK_P: return KEYCODE_P;
        case SDLK_Q: return KEYCODE_Q;
        case SDLK_R: return KEYCODE_R;
        case SDLK_S: return KEYCODE_S;
        case SDLK_T: return KEYCODE_T;
        case SDLK_U: return KEYCODE_U;
        case SDLK_V: return KEYCODE_V;
        case SDLK_W: return KEYCODE_W;
        case SDLK_X: return KEYCODE_X;
        case SDLK_Y: return KEYCODE_Y;
        case SDLK_Z: return KEYCODE_Z;
        
        case SDLK_0: return KEYCODE_0;
        case SDLK_1: return KEYCODE_1;
        case SDLK_2: return KEYCODE_2;
        case SDLK_3: return KEYCODE_3;
        case SDLK_4: return KEYCODE_4;
        case SDLK_5: return KEYCODE_5;
        case SDLK_6: return KEYCODE_6;
        case SDLK_7: return KEYCODE_7;
        case SDLK_8: return KEYCODE_8;
        case SDLK_9: return KEYCODE_9;
        
        case SDLK_ESCAPE: return KEYCODE_ESCAPE;
        case SDLK_RETURN: return KEYCODE_ENTER;
        case SDLK_SPACE:  return KEYCODE_SPACE;
        case SDLK_LSHIFT: return KEYCODE_LEFTSHIFT;
        case SDLK_RSHIFT: return KEYCODE_RIGHTSHIFT;
        case SDLK_LCTRL: return KEYCODE_LEFTCTRL;
        case SDLK_RCTRL: return KEYCODE_RIGHTCTRL;

        case SDLK_UP: return KEYCODE_UPARROW;
        case SDLK_RIGHT: return KEYCODE_RIGHTARROW;
        case SDLK_DOWN: return KEYCODE_DOWNARROW;
        case SDLK_LEFT: return KEYCODE_LEFTARROW;
        
        default: return KEYCODE_UNKNOWN;
    }
}



// --- Helper: Translate SDL Mouse Buttons ---
static MouseButton TranslateMouseButton(Uint8 sdl_button)
{
    switch (sdl_button)
    {
        case SDL_BUTTON_LEFT:   return MOUSE_BUTTON_LEFT;
        case SDL_BUTTON_RIGHT:  return MOUSE_BUTTON_RIGHT;
        case SDL_BUTTON_MIDDLE: return MOUSE_BUTTON_MIDDLE;
        default: return MOUSE_BUTTON_MAX; // Out of bounds/unsupported
    }
}







// --- Platform API Implementation ---

Window* Platform_Init(const char* title, uint32_t width, uint32_t height, GraphicsAPI api)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        // In a real engine, route this to your Log system
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return false;
    }

    Window* win = (Window*)malloc(sizeof(Window));
    if (!win) return NULL;

    win->current_api = api;
    win->gl_context = NULL;

    SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;

    if (api == GRAPHICS_API_OPENGL)
    {
        // Set openGL attributes if using openGL
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        
        window_flags |= SDL_WINDOW_OPENGL;
    }
    else if (api == GRAPHICS_API_VULKAN)
    {
        // TODO implement vulkan stuff, and other API stuff
        window_flags |= SDL_WINDOW_VULKAN;
    }


    win->sdl_window = SDL_CreateWindow(title, width, height, window_flags);
    if (!win->sdl_window)
    {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        return false;
    }

    if (api == GRAPHICS_API_OPENGL)
    {
        win->gl_context = SDL_GL_CreateContext(win->sdl_window);
        if (!win->gl_context)
        {
            printf("SDL_GL_CreateContext Error: %s\n", SDL_GetError());
            return false;
        }
    }
    

    win->width = width;
    win->height = height;
    win->should_close = false;

    return win;
}



void* Platform_GetProcAddress(const char* name)
{
    // SDL3 changed this slightly, but it acts exactly the same
    return (void*)SDL_GL_GetProcAddress(name); 
}





// bool platform_pollEvents(Event* out_event)
// void platform_pollEvents(Window* window)
bool Platform_PollEvents(Event* e)
{
    SDL_Event sdl_event;
    
    // Loop until we find an event we care about, or the queue is empty.
    while (SDL_PollEvent(&sdl_event))
    {
        // Event e = {0};
        // Zero-init our custom event to prevent union garbage
        e->type = EVENT_NONE;

        switch (sdl_event.type)
        {
            case SDL_EVENT_QUIT:
                // window->should_close = true;
                e->type = EVENT_WINDOW_CLOSE;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                e->type = EVENT_WINDOW_RESIZE;
                e->window_resize.width = (uint32_t)sdl_event.window.data1;
                e->window_resize.height = (uint32_t)sdl_event.window.data2;
                break;

            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                e->type = EVENT_WINDOW_FOCUS_GAINED;
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                e->type = EVENT_WINDOW_FOCUS_LOST;
                break;

            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
                // We let our Input Manager handle repeats, so we can ignore SDL's repeat flag
                if (sdl_event.type == SDL_EVENT_KEY_DOWN)
                {
                    e->type = EVENT_KEY_PRESSED;
                }
                else
                {
                    e->type = EVENT_KEY_RELEASED;
                }
                e->key.key = TranslateKey(sdl_event.key.key);
                
                // Only yield if it's a key we actually track
                if (e->key.key == KEYCODE_UNKNOWN)
                    e->type = EVENT_NONE;
                
                break;

            case SDL_EVENT_MOUSE_MOTION:
                e->type = EVENT_MOUSE_MOVED;
                // SDL3 motion is float, our struct expects int32_t
                e->mouse_state.x = (int32_t)sdl_event.motion.x;
                e->mouse_state.y = (int32_t)sdl_event.motion.y;
                e->mouse_state.dx = (int32_t)sdl_event.motion.xrel;
                e->mouse_state.dy = (int32_t)sdl_event.motion.yrel;
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                e->type = (sdl_event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? 
                                  EVENT_MOUSE_BUTTON_PRESSED : EVENT_MOUSE_BUTTON_RELEASED;
                e->mouse_button.button = TranslateMouseButton(sdl_event.button.button);
                
                if (e->mouse_button.button == MOUSE_BUTTON_MAX)
                {
                    e->type = EVENT_NONE;
                }
                break;

            case SDL_EVENT_MOUSE_WHEEL:
                e->type = EVENT_MOUSEWHEEL_SCROLLED;
                e->mouse_scroll.delta_y = sdl_event.wheel.y;
                break;

            default:
                // It's an SDL event we don't care about (e.g., joystick, text input).
                // Do NOT return false here! Just let the while loop grab the next event.
                break;
        }

        if (e->type != EVENT_NONE)
        {
            // Input_ProcessEvent(&e);
            return true;
        }
    }

    return false;
}





void Platform_SwapBuffers(Window* window)
{
    if (!window || !window->sdl_window) return;

    // Only swap buffers if using openGL
    if (window->current_api == GRAPHICS_API_OPENGL)
        SDL_GL_SwapWindow(window->sdl_window);

    Input_Update();
}





void Platform_Shutdown(Window* window)
{
    if (window)
    {
        if (window->gl_context) SDL_GL_DestroyContext(window->gl_context);
        if (window->sdl_window) SDL_DestroyWindow(window->sdl_window);
        free(window);
    }

    SDL_Quit();
}





double Platform_GetTime(void)
{
    // SDL_GetPerformanceCounter is the high-resolution timer in SDL
    uint64_t counter = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    return (double)counter / (double)frequency;
}





void Platform_Delay(uint32_t ms)
{
    SDL_Delay(ms);
}





void Platform_SetRelativeMouseMode(Window* window, bool enabled)
{
    if (!window || !window->sdl_window) return;
    // In SDL3, this traps the cursor and reports relative motion
    SDL_SetWindowRelativeMouseMode(window->sdl_window, enabled);
}





void Platform_WarpMouse(Window* window)
{
    SDL_WarpMouseInWindow(window->sdl_window, window->width/2, window->height/2);
}