#ifndef CORE_IO_H
#define CORE_IO_H

#include "log_core.h"


// Reads an entire file into a null-terminated string.
// The caller is responsible for calling free() on the returned pointer
char* IO_ReadTextFile(const char* filepath);

// Reads an entire file as binary. out_size received the byte count (not including a terminator)
// The caller is responsible for calling free() on the returned pointer
unsigned char* IO_ReadBinaryFile(const char* filepath, int* out_size);


#endif