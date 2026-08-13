#include "io_core.h"
#include <stdio.h>
#include <stdlib.h>



char* IO_ReadTextFile(const char* filepath)
{
    // Open the file in binary read mode
    FILE* file = fopen(filepath, "rb");
    if (!file)
    {
        Log_Error("ERROR: Failed to open file: %s\n", filepath);
        return NULL;
    }

    // Go to the end to get the length of the file
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
    buffer[read_length] = '\0'; // Add the null terminator

    fclose(file);
    
    return buffer;
}





unsigned char* IO_ReadBinaryFile(const char* filepath, int* out_size)
{
    if (out_size)
        *out_size = 0;

    FILE* file = fopen(filepath, "rb");
    if (!file)
    {
        Log_Error("ERROR: Failed to open file: %s\n", filepath);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (length < 0)
    {
        fclose(file);
        return NULL;
    }

    unsigned char* buffer = (unsigned char*)malloc((size_t)length + 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    size_t read_length = fread(buffer, 1, (size_t)length, file);
    buffer[read_length] = '\0';
    fclose(file);

    if (out_size)
        *out_size = (int)read_length;

    return buffer;
}