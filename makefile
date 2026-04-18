# --- OS Detection & Platform Settings ---
ifeq ($(OS),Windows_NT)
    PLATFORM := Windows
    GL_LIBS := -lopengl32
    SHARED_EXT := .dll
    EXE_EXT := .exe
else
    PLATFORM := $(shell uname -s)
    ifeq ($(PLATFORM),Linux)
        GL_LIBS := -lGL -lm
        SHARED_EXT := .so
        EXE_EXT :=
    endif
    ifeq ($(PLATFORM),Darwin)
        GL_LIBS := -framework OpenGL -lm
        SHARED_EXT := .dylib
        EXE_EXT :=
    endif
endif


# --- Compiler & Linker Flags ---
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -g -fPIC -I. -Iinclude -Iinclude/bullet
CXXFLAGS = $(CFLAGS) -std=c++14
LDFLAGS = -Llib -lSDL3 -lBulletDynamics -lBulletCollision -lLinearMath $(GL_LIBS)

# --- Directories & Files ---
SRC_DIRS = . core core/math platform/sdl render/opengl include/glad scene assets
BUILD_DIR = build

C_SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.c))
CPP_SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))

C_OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(C_SRCS))
CPP_OBJS = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(CPP_SRCS))
OBJS = $(C_OBJS) $(CPP_OBJS)



# --- Build Targets ---
TARGET = $(BUILD_DIR)/PrismEngine$(EXE_EXT)
STATIC_LIB = $(BUILD_DIR)/libPrismEngine.a
SHARED_LIB = $(BUILD_DIR)/libPrismEngine$(SHARED_EXT)

# Filter out main.o for library builds
LIB_OBJS = $(filter-out $(BUILD_DIR)/main.o, $(OBJS))



# --- Rules ---

.PHONY: all clean static shared Prism

# Default target when you just type 'make'
Prism: $(TARGET)

static: $(STATIC_LIB)
shared: $(SHARED_LIB)

# Rule for making a static library
$(STATIC_LIB): $(LIB_OBJS)
	@echo "Archiving Static Library: $@"
	@ar rcs $@ $(LIB_OBJS)
	@echo "Static library built successfully!"

# Rule for making a dynamic library
$(SHARED_LIB): $(LIB_OBJS)
	@echo "Linking Shared Library: $@"
	@$(CXX) -shared -o $@ $(LIB_OBJS) $(LDFLAGS)
	@echo "Shared library built successfully!"

# Rule for making test executable
$(TARGET): $(OBJS)
	@echo "Linking $@"
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful! Run with: ./$(TARGET)"

# Rule for compiling all .c files
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule for compiling all .cpp files
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "Compiling C++ $<..."
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Cleans up the build directory
clean:
	@echo "Cleaning build directory..."
	@find $(BUILD_DIR) -type f -name "*.o" -delete
