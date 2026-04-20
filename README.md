# Prism Engine

A 3D engine aimed at being minimal, yet comprehensive for projects. It includes a **C-first** **data-oriented ECS-style scene**, **OpenGL** backend support, **SDL3** support for windowing and input, and **Bullet3** physics for rigid bodies, raycasts, and collision callbacks.

---

## Features

| Feature | Details |
|------|----------------|
| **Scene system** | ECS-style arrays and masks; **transforms** with parent/child hierarchy; **camera**, **point lights**, **render** components; up to **4096** entities |
| **Rendering backends** | API-agnostic **`render/`** layer; **`render/opengl/`** is the active backend. **Vulkan** / **DirectX** / **none** exist as types only |
| **Materials & meshes** | **Custom shaders** and **textures**; material properties (tint, specular, shininess); **OBJ** import, **stb_image**, builtin **quad / cube / sphere** |
| **Physics** | Bullet3 physics integration. Box, sphere, mesh colliders; triggers; collision layers & masks; rigidbodies; Single and multi Raycasts; physics events exposed to scripts |
| **Platform** | **`platform/`** abstracts the OS; **`platform/sdl/`** implements window, GL context, input, time, relative mouse |
| **Custom scripts** | Per-entity function pointers (`OnStart`, `OnUpdate`, collision and trigger hooks); multiple scripts per entity |

---

## Project structure

```
Prism Engine/
├── Prism.c / Prism.h          # Engine lifecycle. Ties all modules.
├── assets/                    # Asset manager (meshes, materials, shaders)
├── core/                      # Core utilities: Math, mesh, input, events, time, log, I/O
├── include/                   # Third-party headers - GLAD, SDL3, Bullet3
├── lib/                       # Prebuilt libraries for linker (provide SDL3 and Bullet3 libraries)
├── platform/                  # Window and OS API decleration and usage - (Currently SDL3 implementation only)
├── render/                    # Render interface and backend implementation (Currently OpenGL only)
└── scene/                     # Scene graph, ECS, physics integration
```

---

## Build

**Toolchain:** **GNU** **GCC** (`gcc`), **G++** (`g++`, **C++14**), and CMake.
On Windows use **MinGW-w64** (e.g. **MSYS2**, or **w64devkit** | MSVC has not been tested)

CMake should automatically fetch SDL3 and Bullet physics if you don't have it.

```bash
cmake -S . -B build          - Generates build files ( additionally add "-G MinGW Makefiles" to ensure mingw is used )

cmake --build build          - Builds engine ( static and dynamic library options are available )
```

Output: **`libPrismEngine.so`** (or **`libPrismEngine.dll`** on Windows).


## Usage (minimal)

Prerequisites:
1. Include Prism headers in an "include" folder (must roughly match project structure). Main Headers: **`Prism.h`**, **`scene/scene.h`**, **`render/render.h`**, **`platform/platform.h`**, **`assets/asset_manager.h`**.
2. Link with Prism lib file (.dll, or .a)

Usage:
1. Initialize engine with **`Engine_Init(..., GRAPHICS_API_OPENGL)`**
2. Initialize scene with **`Scene_Init`**
3. Add entities, cameras, etc. using respective functions.
4. Each frame: **`Engine_IsRunning()`** (in main loop) → **`Scene_Update`** → **`Render_Clear`** → **`Engine_RenderScene`** → **`Engine_EndFrame`**
5. **`Engine_Shutdown()`**




## Run

All assets used are relative to the executable (unless providing an absolute path). No sample textures or models provided, so add your own.

---

## Future plans

Implementing Vulkan/DirectX and headless backends. Tooling around the asset and ECS layer (editor). More thorough C/C++ scripting API.

---

*Still highly experimental — APIs will change.*
