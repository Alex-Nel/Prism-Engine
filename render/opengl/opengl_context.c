#include "opengl_context.h"
#include "SDL3/SDL.h"

#include <stddef.h>





// Initializes the openGL surface using SDL
bool OpenGL_Surface_Init(OpenGL_Surface* surface, void* native_window)
{
    if (!surface || !native_window)
        return false;

    SDL_Window* window = (SDL_Window*)native_window;
    SDL_GLContext context = SDL_GL_CreateContext(window);
    
    if (!context)
        return false;
    
    if (!SDL_GL_MakeCurrent(window, context))
    {
        SDL_GL_DestroyContext(context);
        return false;
    }
    
    surface->native_window = native_window;
    surface->gl_context = context;
    return true;
}










// Shuts down the OpenGL context
void OpenGL_Surface_Shutdown(OpenGL_Surface* surface)
{
    if (!surface)
        return;

    if (surface->native_window && surface->gl_context)
    {
        SDL_Window* window = (SDL_Window*)surface->native_window;
        SDL_GL_MakeCurrent(window, NULL);
        SDL_GL_DestroyContext((SDL_GLContext)surface->gl_context);
    }
    
    surface->native_window = NULL;
    surface->gl_context = NULL;
}










// Makes a specific opengl surface the current for the window
bool OpenGL_Surface_MakeCurrent(OpenGL_Surface* surface)
{
    if (!surface || !surface->native_window || !surface->gl_context)
        return false;

    return SDL_GL_MakeCurrent((SDL_Window*)surface->native_window, (SDL_GLContext)surface->gl_context);
}










// Releases the current openGL context
void OpenGL_Surface_ReleaseCurrent(OpenGL_Surface* surface)
{
    (void)surface;
    SDL_GL_MakeCurrent(NULL, NULL);
}










// Presents the OpenGL surface to the current window
void OpenGL_Surface_Present(OpenGL_Surface* surface)
{
    if (!surface || !surface->native_window)
        return;
    SDL_GL_SwapWindow((SDL_Window*)surface->native_window);
}










// Enabled/Disables VSync for an OpenGL surface
void OpenGL_Surface_SetVSync(OpenGL_Surface* surface, bool enabled)
{
    (void)surface;
    SDL_GL_SetSwapInterval(enabled ? 1 : 0);
}










// Gets the proc address of the OpenGL surface
void* OpenGL_Surface_GetProcAddress(const char* name)
{
    return (void*)SDL_GL_GetProcAddress(name);
}