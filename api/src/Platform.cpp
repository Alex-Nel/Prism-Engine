#include "../include/Platform.hpp"


extern "C"
{
    #include "../../platform/platformCore.h"
}


namespace Prism 
{
    double Platform::GetTime() 
    {
        return ::Platform_GetTime();
    }

    void Platform::SetRelativeMouseMode(void* window, bool enabled) 
    {
        // Cast the opaque pointer back to the C struct
        ::Platform_SetRelativeMouseMode(static_cast<::Window*>(window), enabled);
    }

    void Platform::WarpMouse(void* window) 
    {
        ::Platform_WarpMouse(static_cast<::Window*>(window));
    }
}