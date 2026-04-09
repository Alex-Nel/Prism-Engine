#include "log.h"
#include <stdio.h>
#include <stdarg.h>



void Log_Message(LogLevel level, const char* file, int line, const char* format, ...)
{
    const char* level_strings[] = {"INFO", "WARN", "ERROR", "DEBUG"};
    
    // Print the header
    printf("[%s] %s:%d: ", level_strings[level], file, line);
    
    // Handle the variadic arguments
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");

    // NOTE: For other platforms, more will need to be added to route output to correct function
}