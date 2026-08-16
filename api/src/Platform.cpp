#include "../include/Platform.hpp"


extern "C"
{
    #include "../../platform/platform_core.h"
}


namespace Prism 
{
    double Platform::GetTime() {
        return ::Platform_GetTime();
    }

    void Platform::SetRelativeMouseMode(bool enabled) {
        // Cast the opaque pointer back to the C struct
        ::Platform_SetRelativeMouseMode(static_cast<::Window*>(GetActiveWindow()), enabled);
    }

    void Platform::WarpMouseToMiddle() {
        ::Platform_WarpMouseToMiddle(static_cast<::Window*>(GetActiveWindow()));
    }

    bool Platform::SetClipboardText(const std::string& text) {
        return ::Platform_SetClipboardText(text.c_str());
    }

    std::string Platform::GetClipboardText() {
        char* text = ::Platform_GetClipboardText();
        if (!text) return {};

        std::string result(text);
        ::Platform_FreeClipboardText(text);
        return result;
    }

    int Platform::GetWindowWidth() {
        return ::Platform_GetWindowWidth(static_cast<::Window*>(GetActiveWindow()));
    }

    int Platform::GetWindowHeight() {
        return ::Platform_GetWindowHeight(static_cast<::Window*>(GetActiveWindow()));
    }

    bool Platform::IsWindowMinimized() {
        return ::Platform_IsWindowMinimized(static_cast<::Window*>(GetActiveWindow()));
    }
}