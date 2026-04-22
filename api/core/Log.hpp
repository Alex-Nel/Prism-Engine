#pragma once

extern "C"
{
    #include "log.h"
}

// We wrap the C macros in Prism-specific macros.
// This ensures that when a user calls PRISM_ERROR, the C++ preprocessor 
// injects their specific script's file and line number into the C backend.

#define Debug_Info(...)  Log_Info(__VA_ARGS__)
#define Debug_Warn(...)  Log_Warning(__VA_ARGS__)
#define Debug_Error(...) Log_Error(__VA_ARGS__)
#define Debug_Debug(...) Log_Debug(__VA_ARGS__)

/* 
   Note: If you eventually want to support C++ std::string formatting 
   instead of printf formatting, you would integrate a library like {fmt} 
   here in the future. For now, standard printf variadics work perfectly.
*/