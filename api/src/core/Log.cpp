#include "../../include/core/Log.hpp"
#include <cstdarg>
#include <cstdio>


extern "C"
{
    #include "../../../core/log_core.h"
}


namespace Prism 
{
    // Logs information to the console
    void Debug::Log(const char* file, int line, const char* format, ...) 
    {
        va_list args;

        va_start(args, format);
        Log_MessageV(LOG_LEVEL_INFO, file, line, format, args);
        va_end(args);
    }


    // Logs a warning to the console
    void Debug::Warning(const char* file, int line, const char* format, ...) 
    {
        va_list args;

        va_start(args, format);
        Log_MessageV(LOG_LEVEL_WARN, file, line, format, args);
        va_end(args);
    }


    // Logs an error to the console
    void Debug::Error(const char* file, int line, const char* format, ...) 
    {
        va_list args;

        va_start(args, format);
        Log_MessageV(LOG_LEVEL_ERROR, file, line, format, args);
        va_end(args);
    }
}