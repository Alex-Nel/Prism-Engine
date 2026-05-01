cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Forgive older CMake versions" FORCE)
project(PrismEngine C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- USER OPTIONS ---
option(PRISM_BUILD_TEST "Build the main.c test executable (if it exists)" ON)
option(BUILD_SHARED_LIBS "Build PrismEngine as a shared library (.dll / .so)" OFF)

# --- DEPENDENCY FETCHING ---
include(FetchContent)

message(STATUS "Fetching SDL3 from GitHub...")
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG preview-3.1.6
    GIT_SHALLOW TRUE
)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)



message(STATUS "Fetching Bullet3 from GitHub...")
FetchContent_Declare(
    bullet3
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG 3.25
    GIT_SHALLOW TRUE
)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(bullet3)



message(STATUS "Fetching cJSON from GitHub...")
FetchContent_Declare(
    cJSON
    GIT_REPOSITORY https://github.com/DaveGamble/cJSON.git
    GIT_TAG v1.7.18
    GIT_SHALLOW TRUE
)
set(ENABLE_CJSON_UNINSTALL OFF CACHE BOOL "" FORCE) # Prevent cJSON from creating an "uninstall" target that collides with SDL3's
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE) # Turn off its internal tests so it builds instantly
set(ENABLE_CUSTOM_COMPILER_FLAGS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cJSON)

find_package(OpenGL REQUIRED)



# --- SOURCE FILES ---

# 1. Internal systems
file(GLOB_RECURSE ENGINE_SRCS
    "core/*.c" "core/*.cpp"
    "platform/sdl/*.c" "platform/sdl/*.cpp"
    "render/opengl/*.c" "render/opengl/*.cpp"
    "scene/*.c" "scene/*.cpp"
    "assets/*.c" "assets/*.cpp"
    "include/glad/*.c"
    "Prism.c"
)

# 2. The Public API Implementation (C++ headers)
file(GLOB_RECURSE API_SRCS "api/src/*.cpp")



# --- Target 1: Engine Library ---
add_library(PrismEngineLib ${ENGINE_SRCS} ${API_SRCS})

# PUBLIC: The user is ONLY allowed to use the C++ headers!
target_include_directories(PrismEngineLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/api/include
)

# PRIVATE: These are beeded to compile, but they're blocked from the user.
target_include_directories(PrismEngineLib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}         # Allows api/src/ to find "scene/scene.h"
    ${CMAKE_CURRENT_SOURCE_DIR}/include # Internal vendor headers (glad, stb)
    ${bullet3_SOURCE_DIR}/src           # Hidden physics library
    ${cjson_SOURCE_DIR}                 # Hidden JSON library
)

# PRIVATE: The engine uses these libraries natively.
target_link_libraries(PrismEngineLib PRIVATE
    SDL3::SDL3
    BulletDynamics
    BulletCollision
    LinearMath
    OpenGL::GL
    cjson
)

# --- Target 2: Test Executable ---
if(PRISM_BUILD_TEST)
    # Only try to build the test if a main.cpp exists
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
        add_executable(PrismTest main.cpp)
        
        # When we link PrismEngineLib, CMake automatically gives  PUBLIC includes (api/include)
        target_link_libraries(PrismTest PRIVATE PrismEngineLib)
    else()
        message(STATUS "main.cpp not found. Skipping test executable build.")
    endif()
endif()