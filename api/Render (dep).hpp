#pragma once

#include "core/Math.hpp"

extern "C"
{
    #include "render.h"
}

namespace Prism
{
    // ==========================================
    // MATERIAL PROPERTIES WRAPPER
    // ==========================================
    struct MaterialProperties : public ::MaterialProperties
    {    
        MaterialProperties() {
            tint_color = {1.0f, 1.0f, 1.0f};
            shininess = 32.0f;
            specular_strength = 0.5f;
        }

        MaterialProperties(const Prism::Vector3& color, float shine, float specular) {
            tint_color = color;
            shininess = shine;
            specular_strength = specular;
        }

        MaterialProperties(const ::MaterialProperties& raw) {
            tint_color = raw.tint_color;
            shininess = raw.shininess;
            specular_strength = raw.specular_strength;
        }

        operator ::MaterialProperties() const { return *this; }
    };



    // ==========================================
    // RENDERER INTERFACE (Safe Functions Only)
    // ==========================================
    class Renderer
    {
    public:
        Renderer() = delete;

        // --- Safe Global Configurations ---

        // Changes the background clear color of the screen
        static void SetClearColor(const Prism::Vector3& color, float alpha = 1.0f) {
            ::Render_SetClearColor(color.x, color.y, color.z, alpha);
        }

        // --- DO NOT EXPOSE ---
        // Render_Init
        // Render_Submit
        // Render_BeginFrame
        // Render_EndFrame
        // These belong to the core engine loop!
    };
}