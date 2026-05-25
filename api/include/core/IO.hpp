#pragma once

#include <string>
#include "../PrismAPI.hpp"


namespace Prism
{
    class PRISM_API IO
    {
    public:
        // Prevent instantiation
        IO() = delete;

        // Reads all the text from a file (assuming format is text based)
        static std::string ReadTextFile(const std::string& filepath);
    };
}