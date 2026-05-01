#include "../../include/core/IO.hpp"


extern "C"
{
    #include "../../../core/ioCore.h"
    #include <stdlib.h> 
}


namespace Prism
{
    std::string IO::ReadTextFile(const std::string& filepath) 
    {
        char* raw_buffer = ::IO_ReadTextFile(filepath.c_str());
        
        if (!raw_buffer)
            return std::string(""); 

        std::string result(raw_buffer);
        free(raw_buffer);

        return result;
    }
}