#pragma once

#include "../PrismAPI.hpp"



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

}