#include "../../include/core/Log.hpp"
#include <cstdarg>
#include <cstdio>


extern "C"
{
    #include "../../../core/log.h"
}


namespace Prism 
{
    // Logs information to the console
    void Debug::Log(const char* format, ...) 
    {
        // Format the user's string
        char buffer[8192];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Pass to the backend macro
        Log_Info("%s", buffer);
    }


    // Logs a warning to the console
    void Debug::Warning(const char* format, ...) 
    {
        // Format the user's string
        char buffer[8192];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Pass to the backend macro
        Log_Warning("%s", buffer);
    }


    // Logs an error to the console
    void Debug::Error(const char* format, ...) 
    {
        // Format the user's string
        char buffer[8192];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Pass to the backend macro
        Log_Error("%s", buffer);
    }
}