# --- Compiler Settings ---
CC = gcc
CXX = g++

# -I. tells the compiler to look in the current root directory for your own headers.
# -Iinclude tells the compiler to look in your new include folder for third-party headers (like SDL3).
CFLAGS = -Wall -Wextra -g -I. -Iinclude -Iinclude/bullet -Llib
CXXFLAGS = $(CFLAGS) -std=c++14

# --- Linker Settings ---
# -lSDL3 links the SDL3 library. -lm links the standard C math library.
LDFLAGS = -Llib -lSDL3 -lm -lopengl32 -lBulletDynamics -lBulletCollision -lLinearMath

# --- Directories ---
# Add any folders containing .c files to this list
SRC_DIRS = . core core/math platform/sdl render/opengl include/glad scene assets
BUILD_DIR = build

# --- Files ---
# Automatically find all .c files in the listed directories
C_SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
CPP_SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))

# Map the .c filenames to .o filenames inside the build directory
C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
CPP_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CPP_SRCS))

OBJS = $(C_OBJS) $(CPP_OBJS)

# Name of your final executable
# TARGET = $(BUILD_DIR)/../PrismEngine
TARGET = $(BUILD_DIR)/PrismEngine

# --- Rules ---

# Default target when you just type 'make'
all: $(TARGET)

# Rule to link all object files into the final executable
$(TARGET): $(OBJS)
	@echo "Linking $@"
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful! Run with: ./$(TARGET)"

# Rule to compile .c files into .o files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile .cpp files into .o files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling C++ $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Cleans up the build directory
clean:
	@echo "Cleaning build directory..."
	@find $(BUILD_DIR) -type f -name "*.o" -delete
# @rm -rf $(BUILD_DIR)

.PHONY: all clean