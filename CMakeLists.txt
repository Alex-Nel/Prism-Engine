cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Forgive older CMake versions" FORCE)
project(PrismEngine C CXX)


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)


add_compile_options(-Wno-unknown-pragmas)
add_compile_options(-Wno-error=array-bounds)


# Default to Release mode if the user didn't specify.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()


# --- User Options ---
option(PRISM_BUILD_TEST "Build the main.c test executable (if it exists)" ON)
option(PRISM_BUILD_SHARED_LIBS "Build PrismEngine as a shared library (.dll / .so)" ON)


# --- Dependency Packing ---
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Force all dependencies below to build as STATIC libraries





# --- Dependency Fetching ---
include(FetchContent)

message(STATUS "Fetching SDL3 from GitHub...")
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.10
    GIT_SHALLOW TRUE
)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)



message(STATUS "Fetching Box3D from GitHub...")
FetchContent_Declare(
    box3d
    GIT_REPOSITORY https://github.com/erincatto/box3d.git
    GIT_TAG v0.1.0
    GIT_SHALLOW TRUE
)
set(BOX3D_SAMPLES OFF CACHE BOOL "" FORCE)
set(BOX3D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX3D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX3D_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(box3d)



message(STATUS "Fetching cJSON from GitHub...")
FetchContent_Declare(
    cJSON
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG v1.7.18
    GIT_SHALLOW TRUE
)
set(ENABLE_CJSON_UNINSTALL OFF CACHE BOOL "" FORCE) # Prevent cJSON from creating an "uninstall" target that collides with SDL3's
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE) # Turn off internal tests
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cJSON)

find_package(OpenGL REQUIRED)



message(STATUS "Fetching Assimp from GitHub...")
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v6.0.5
    GIT_SHALLOW TRUE
)
set(ASSIMP_BUILD_WERROR OFF CACHE BOOL "Build with Werror" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE) # Required for FBX/GLTF
set(INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF)
FetchContent_MakeAvailable(assimp)


# Restore user choices from before
set(BUILD_SHARED_LIBS ${PRISM_BUILD_SHARED_LIBS} CACHE BOOL "" FORCE)





# --- Source Files ---

# 1. Internal systems
file(GLOB_RECURSE ENGINE_SRCS
    "core/*.c" "core/*.cpp"
    "audio/*.c" "audio/*.cpp"
    "platform/sdl/*.c" "platform/sdl/*.cpp"
    "render/*.c" "render/*.cpp"
    "render/opengl/*.c" "render/opengl/*.cpp"
    "render/headless/*.c" "render/headless/*.cpp"
    "scene/*.c" "scene/*.cpp"
    "assets/*.c" "assets/*.cpp"
    "include/glad/*.c"
    "Prism.c"
)

# 2. The Public API Implementation (C++ headers)
file(GLOB_RECURSE API_SRCS "api/src/*.cpp")





# --- Target 1: Engine Library ---

# If building a DLL, export the symbols
if(PRISM_BUILD_SHARED_LIBS)
    add_library(PrismEngineLib SHARED ${ENGINE_SRCS} ${API_SRCS})
    target_compile_definitions(PrismEngineLib PRIVATE PRISM_BUILD_DLL)
else()
    add_library(PrismEngineLib STATIC ${ENGINE_SRCS} ${API_SRCS})
endif()

# PUBLIC: The user is ONLY allowed to use the C++ headers
target_include_directories(PrismEngineLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/api/include
)

# PRIVATE: These are needed to compile, but they're blocked from the user.
target_include_directories(PrismEngineLib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}         # Allows api source to include needed headers
    ${CMAKE_CURRENT_SOURCE_DIR}/include # Hides internal vendor headers (glad, stb)
    ${box3d_SOURCE_DIR}/src             # Hides physics library
    ${cjson_SOURCE_DIR}                 # Hides JSON library
)

# PRIVATE: The engine uses these libraries natively.
target_link_libraries(PrismEngineLib PRIVATE
    SDL3::SDL3
    box3d::box3d
    OpenGL::GL
    cjson
    assimp
)



# --- Target 2: Test Executable ---
if(PRISM_BUILD_TEST)
    # Only try to build the test if a main.cpp exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
        add_executable(PrismTest main.cpp)
        
        # When we link PrismEngineLib, CMake automatically gives PUBLIC includes (api/include)
        target_link_libraries(PrismTest PRIVATE PrismEngineLib)
    else()
        message(STATUS "main.cpp not found. Skipping test executable build.")
    endif()
endif()