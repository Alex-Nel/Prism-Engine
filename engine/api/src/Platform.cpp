#include "Platform.hpp"


extern "C"
{
    #include "platform/platform_core.h"
    #include "core/image_core.h"
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

    bool Platform::SetWindowIcon(const std::string& filepath) {
        // Window icons expect top-down orientation (Y=0 at top), so inverted must be false.
        ::ImageData raw = ::Image_Load(filepath.c_str(), false);
        if (!raw.pixels)
            return false;

        bool result = ::Platform_SetWindowIcon(
            static_cast<::Window*>(GetActiveWindow()),
            raw.pixels,
            static_cast<uint32_t>(raw.width),
            static_cast<uint32_t>(raw.height),
            static_cast<uint32_t>(raw.channels)
        );

        ::Image_Free(&raw);
        return result;
    }

    bool Platform::SetWindowIcon(const Image& image) {
        if (!image.pixels || image.width <= 0 || image.height <= 0 || image.channels <= 0)
            return false;

        return ::Platform_SetWindowIcon(
            static_cast<::Window*>(GetActiveWindow()),
            image.pixels,
            static_cast<uint32_t>(image.width),
            static_cast<uint32_t>(image.height),
            static_cast<uint32_t>(image.channels)
        );
    }

    bool Platform::SetWindowIcon(const void* pixels, uint32_t width, uint32_t height, uint32_t channels) {
        return ::Platform_SetWindowIcon(static_cast<::Window*>(GetActiveWindow()), pixels, width, height, channels);
    }
}