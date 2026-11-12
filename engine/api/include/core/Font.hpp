#pragma once

#include "PrismAPI.hpp"
#include "Color.hpp"
#include <cstdio>
#include <string>



namespace Prism
{

	// ==========================================
    // Font Wrapper
    // ==========================================

    class PRISM_API Font
    {
    private:
        void* m_Handle;

    public:
        Font(void* raw_font = nullptr) : m_Handle(raw_font) {}

        void* GetRaw() const {
            return m_Handle;
        }

        bool IsValid() const {
            return m_Handle != nullptr;
        }
    };



    // ==========================================
    // Text Scope
    // ==========================================

    #define MAX_TEXT_LENGTH 256
    
    enum class TextHorizontalAlign
    {
        Left = 0,
        Center = 1,
        Right = 2
    };
    
    enum class TextVerticalAlign
    {
        Top = 0,
        Center = 1,
        Bottom = 2
    };
    
    // Mirrors the core TextScope layout
    struct PRISM_API TextScope
    {
        char text[MAX_TEXT_LENGTH];
        Prism::Color color;
        Prism::Font font;
        float font_size;

        void SetText(const std::string& value) { std::snprintf(text, MAX_TEXT_LENGTH, "%s", value.c_str()); }
        std::string GetText() const { return std::string(text); }
    };
}