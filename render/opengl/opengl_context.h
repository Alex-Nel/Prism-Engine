#ifndef OPENGL_CONTEXT_H
#define OPENGL_CONTEXT_H


#include <stdbool.h>



// Struct for the OpenGL window and context
typedef struct OpenGL_Surface
{
    void* native_window;
    void* gl_context;
} OpenGL_Surface;





bool OpenGL_Surface_Init(OpenGL_Surface* surface, void* native_window);
void OpenGL_Surface_Shutdown(OpenGL_Surface* surface);
bool OpenGL_Surface_MakeCurrent(OpenGL_Surface* surface);
void OpenGL_Surface_ReleaseCurrent(OpenGL_Surface* surface);
void OpenGL_Surface_Present(OpenGL_Surface* surface);
void OpenGL_Surface_SetVSync(OpenGL_Surface* surface, bool enabled);
void* OpenGL_Surface_GetProcAddress(const char* name);





#endif // OPENGL_CONTEXT_H