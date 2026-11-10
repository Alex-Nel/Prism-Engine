cmake_minimum_required(VERSION 3.24)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "Forgive older CMake versions" FORCE)
project(PrismEngine C CXX)


set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)


if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-unknown-pragmas)
    add_compile_options(-Wno-error=array-bounds)
endif()


# Default to Release mode if the user didn't specify.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()


# --- User Options ---
option(PRISM_BUILD_TEST "Build the test executables (if they exist)" ON)
option(PRISM_BUILD_SHARED_LIBS "Build PrismEngine as a shared library (.dll / .so)" ON)


# --- Dependency Packing ---
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Force all dependencies below to build as STATIC libraries





# --- Dependency Fetching ---
include(FetchContent)



find_package(OpenGL REQUIRED)



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
    "engine/core/*.c" "engine/core/*.cpp"
    "engine/audio/*.c" "engine/audio/*.cpp"
    "engine/platform/sdl/*.c" "engine/platform/sdl/*.cpp"
    "engine/render/*.c" "engine/render/*.cpp"
    "engine/render/opengl/*.c" "engine/render/opengl/*.cpp"
    "engine/render/headless/*.c" "engine/render/headless/*.cpp"
    "engine/scene/*.c" "engine/scene/*.cpp"
    "engine/assets/*.c" "engine/assets/*.cpp"
    "engine/runtime/*.c"
    "external/glad/*.c"
    "external/cJSON/*.c"
)

# 2. The Public API Implementation (C++ headers)
file(GLOB_RECURSE API_SRCS "engine/api/src/*.cpp")





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
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/api/include
)

# PRIVATE: These are needed to compile, but they're blocked from the user.
target_include_directories(PrismEngineLib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}          # Allows api source to include needed headers
    ${CMAKE_CURRENT_SOURCE_DIR}/engine   # Allows engine modules to include each other
    ${CMAKE_CURRENT_SOURCE_DIR}/external # Allows direct inclusion of third-party headers
    ${box3d_SOURCE_DIR}/src              # Hides physics library
)

# PRIVATE: The engine uses these libraries natively.
target_link_libraries(PrismEngineLib PRIVATE
    SDL3::SDL3
    box3d::box3d
    OpenGL::GL
    assimp
)



# --- Target 2: Test Executables ---
# Compiles each .cpp file in tests/ as its own executable
if(PRISM_BUILD_TEST)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")

    if(TEST_SOURCES)
        foreach(TEST_SRC ${TEST_SOURCES})
            # Get the filename without extension
            get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

            # If there's a file named "main", rename it to "MainTest", and keep others named after their file
            if(TEST_NAME STREQUAL "main")
                set(TARGET_NAME "MainTest")
            else()
                set(TARGET_NAME "${TEST_NAME}")
            endif()

            add_executable(${TARGET_NAME} ${TEST_SRC})
            target_link_libraries(${TARGET_NAME} PRIVATE PrismEngineLib)
        endforeach()
    else()
        message(STATUS "No .cpp files found in tests/. Skipping test executables.")
    endif()
endif()



# --- Target 3: Editor Application ---
# Create an Editor target that links to the engine but defines PRISM_EDITOR
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/editor/main.cpp")
    file(GLOB_RECURSE EDITOR_SRCS
        "tools/editor/*.cpp" "tools/editor/*.c"
    )
    add_executable(PrismEditor ${EDITOR_SRCS})
    
    target_link_libraries(PrismEditor PRIVATE PrismEngineLib)
    target_compile_definitions(PrismEditor PRIVATE PRISM_EDITOR)

    # Compile engine with PRISM_EDITOR for these symbols to exist
    # Implement a CMake Option later.
    target_compile_definitions(PrismEngineLib PUBLIC PRISM_EDITOR)
endif()