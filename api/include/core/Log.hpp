#pragma once


namespace Prism
{
    // --- Class that contains debug functions ---
    class Debug
    {
    public:
        static void Log(const char* format, ...);
        static void Warning(const char* format, ...);
        static void Error(const char* format, ...);
    };
}

// Prism-specific macros mapping to the forward declarations
#define Debug_Log(...)     Prism::Debug::Log(__FILE__, __LINE__, __VA_ARGS__)
#define Debug_Warning(...) Prism::Debug::Warning(__FILE__, __LINE__, __VA_ARGS__)
#define Debug_Error(...)   Prism::Debug::Error(__FILE__, __LINE__, __VA_ARGS__)


// ---------------------------------------------------------
// LEGACY ALIASES
// ---------------------------------------------------------
#define Debug_Info(...)    Prism::Debug::Log(__FILE__, __LINE__, __VA_ARGS__)
#define Debug_Warn(...)    Prism::Debug::Warning(__FILE__, __LINE__, __VA_ARGS__)