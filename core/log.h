#ifndef CORE_LOG_H
#define CORE_LOG_H

#include <stdio.h>

typedef enum LogLevel
{
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_DEBUG
} LogLevel;


#define Log_Info(...)  Log_Message(LOG_LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__)
#define Log_Error(...) Log_Message(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define Log_Warning(...) Log_Message(LOG_LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__)
#define Log_Debug(...) Log_Message(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)


void Log_Message(LogLevel level, const char* file, int line, const char* format, ...);


#endif