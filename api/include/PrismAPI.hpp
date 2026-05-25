#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #ifdef PRISM_BUILD_DLL 
        // We are building the DLL, export the symbols
        #define PRISM_API __declspec(dllexport)
    #else
        // The user is compiling their game, import the symbols
        #define PRISM_API __declspec(dllimport)
    #endif
#else
    // Mac/Linux export everything automatically
    #define PRISM_API
#endif