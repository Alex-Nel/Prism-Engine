#include "io.h"
#include <stdio.h>
#include <stdlib.h>

char* IO_ReadTextFile(const char* filepath)
{
    // Open the file in binary read mode
    FILE* file = fopen(filepath, "rb");
    if (!file)
    {
        printf("Failed to open file: %s\n", filepath);
        return NULL;
    }

    // Seek to the end to find out how big the file is
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET); // Rewind back to the start

    // Allocate memory (+1 for the null terminator)
    char* buffer = (char*)malloc(length + 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    // Read the data into the buffer
    size_t read_length = fread(buffer, 1, length, file);
    buffer[read_length] = '\0'; // Add the null terminator so OpenGL knows where the string ends

    fclose(file);
    return buffer;
}