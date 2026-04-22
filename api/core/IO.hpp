#pragma once

#include <string>

extern "C"
{
    #include "ioCore.h"
    #include <stdlib.h> // Required for free()
}

namespace Prism
{
    class IO
    {
    public:
        // Prevent instantiation
        IO() = delete;

        // Automatically handles the malloc/free lifecycle from the engine
        static std::string ReadTextFile(const std::string& filepath) {
            
            // Call the function
            char* raw_buffer = ::IO_ReadTextFile(filepath.c_str());
            
            // Check for failure
            if (!raw_buffer)
                return std::string(""); 

            // Copy the data into a std::string
            std::string result(raw_buffer);

            // Clean up the C memory so the user doesn't have to
            free(raw_buffer);

            return result;
        }
    };
}