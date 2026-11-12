#include "core/Font.hpp"


extern "C"
{
    #include "core/font_core.h"
}


namespace Prism
{
    void TextScope::Reset() {
        ::TextScope_Init(reinterpret_cast<::TextScope*>(this));
    }

    void TextScope::SetText(const std::string& value) {
        ::TextScope_SetText(reinterpret_cast<::TextScope*>(this), value.c_str());
    }
}