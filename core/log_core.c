#include "log_core.h"
#include <stdarg.h>



// Logs a message to the console (with a variadic list)
void Log_MessageV(LogLevel level, const char* file, int line, const char* format, va_list args)
{
    const char* level_strings[] = {"INFO", "WARN", "ERROR", "DEBUG"};

    printf("[%s] [%s:%d] - ", level_strings[level], file, line);

    vprintf(format, args);

    printf("\n");
}





// Logs a message to the console
void Log_Message(LogLevel level, const char* file, int line, const char* format, ...)
{       
    // Handle the variadic arguments
    va_list args;

    va_start(args, format);
    Log_MessageV(level, file, line, format, args);
    va_end(args);
    
    printf("\n");

    // NOTE: For other platforms, more will need to be added to route output to correct function
}