#pragma once

#include "../PrismAPI.hpp"



namespace Prism
{
	// ==========================================
    // Color Structure
    // ==========================================

    struct PRISM_API Color
    {
        float r, g, b, a;

        // --- Constructors --- 

        Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
        Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
        

        // --- static preset colors ---
        
        static Color White()   { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Black()   { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color Red()     { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static Color Orange()  { return Color(1.0f, 0.65f, 0.0f, 1.0f);}
        static Color Yellow()  { return Color(1.0f, 1.0f, 0.0f, 1.0f); }
        static Color Green()   { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static Color Blue()    { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static Color Indigo()  { return Color(0.3f, 0.0f, 0.5f, 1.0f); }
        static Color Violet()  { return Color(0.5f, 0.0f, 1.0f, 1.0f); }
        static Color Clear()   { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
    };
}