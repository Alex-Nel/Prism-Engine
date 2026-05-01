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
    void Debug::Log(const char* file, int line, const char* format, ...) 
    {
        // Format the user's string
        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Prepend the file and line number from the macro
        char msg[8192];
        snprintf(msg, sizeof(msg), "[%s:%d] %s\n", file, line, buffer);

        // Pass to the backend macro
        printf("[INFO] %s", msg);
    }


    // Logs a warning to the console
    void Debug::Warning(const char* file, int line, const char* format, ...) 
    {
        // Format the user's string
        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Prepend the file and line number from the macro
        char msg[8192];
        snprintf(msg, sizeof(msg), "[%s:%d] %s\n", file, line, buffer);

        // Pass to the backend macro
        printf("[WARNING] %s", msg);
    }


    // Logs an error to the console
    void Debug::Error(const char* file, int line, const char* format, ...) 
    {
        // Format the user's string
        char buffer[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        // Prepend the file and line number from the macro
        char msg[8192];
        snprintf(msg, sizeof(msg), "[%s:%d] %s\n", file, line, buffer);

        // Pass to the backend macro
        fprintf(stderr, "[ERROR] %s", msg);
    }
}