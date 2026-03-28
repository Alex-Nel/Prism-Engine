#ifndef CORE_IO_H
#define CORE_IO_H

#include <stdbool.h>

// Reads an entire file into a null-terminated string.
// The caller is responsible for calling free() on the returned pointer!
char* IO_ReadTextFile(const char* filepath);

#endif