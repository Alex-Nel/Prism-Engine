# Prism Engine

A 3D engine aimed at being minimal, yet comprehensive for projects. It includes a **C-first** **data-oriented ECS-style scene**, **OpenGL** backend support, **SDL3** support for windowing and input, **Bullet3** physics for rigid bodies, raycasts, and collision callbacks, and **Open Asset Importer** for asset management.

---

## Features

| Feature | Details |
|------|----------------|
| **Scene system** | ECS-style arrays and masks; **transforms** with parent/child hierarchy; Several built in components; up to **4096** entities |
| **Rendering backends** | API-agnostic **render** layer; Currently only **OpenGL** is the active backend. **Vulkan** / **DirectX** / **none** exist as types only |
| **Materials & meshes** | **Custom shaders** and **textures**; material properties (tint, specular, shininess); Builtin **quad / cube / sphere** meshes. Custom meshes/models using **Open Asset Importer** |
| **Physics** | **Bullet3** physics integration. Box, sphere, mesh colliders; triggers; collision layers & masks; rigidbodies; Single and multi Raycasts; physics events exposed to scripts |
| **Custom scripts** | Easy to use C++ scripting API. Per-entity function pointers (`OnStart`, `OnUpdate`, collision and trigger hooks); |

---

## Project structure

```
Prism Engine/
├── api/                       # Headers and implementation for the C++ API
├── assets/                    # Asset manager (models, materials, textures, and shaders)
├── audio/                     # Audio Management and miniaudio connection
├── core/                      # Core utilities: Math, mesh, input, events, time, log, I/O
├── include/                   # Third-party headers - GLAD, miniaudio, std_image
├── platform/                  # Window and OS API decleration and usage - (Currently SDL3 implementation only)
├── render/                    # Render interface and backend implementation (Currently OpenGL only)
├── scene/                     # Scene system, ECS, physics integration
└── Prism.c / Prism.h          # Engine lifecycle. Ties all modules.
```

---

## Build Instructions

**Toolchain:** **GNU** **GCC** (`gcc`), **G++** (`g++`, **C++17**), and CMake.
On Windows **MinGW-w64** is recommended (e.g. **MSYS2**, or **w64devkit** | MSVC has not been tested)

CMake should automatically fetch SDL3, Bullet, cJSON, and AssImp if you don't have it.

```bash
mkdir build                  - Creates a build folder

cmake -S . -B build          - Generates build files ( additionally add "-G MinGW Makefiles" to ensure mingw is used on Windows, and -DCMAKE_BUILD_TYPE=Debug to do debug builds )

cmake --build build          - Builds engine ( static and dynamic library options are available )
```

Output: **`libPrismEngine.so`** (or **`libPrismEngine.dll`** on Windows), or **`libPrismEngine.a`** if building static.


## Usage (minimal)

Prerequisites:
1. Include Prism project structure in an "include" folder (must roughly match project structure). All C++ Headers are in **`api/`**. **`api/Prism.hpp`** includes all other headers.
2. Link with Prism lib file (.dll). If linking statically, you'll need to link with SDL3, Bullet3 lib files (BulletCollision, BulletDynamics, LinearMath), AssImp, and cJSON.

Usage:
1. Initialize engine with **`Prism::Engine::Init()`**
2. Initialize scene with **`Prism::Scene::Create()`**
3. Add entities, components, custom scripts, etc. using respective functions.
4. Run the engine with **`Prism::Engine::Run()`**.
5. When exiting, run **`Prism::Engine::Shutdown()`**




## Running Engine

All assets used are relative to the executable's path (unless providing an absolute path). No sample textures or models provided, so add your own.

---

## Future plans

Implementing Vulkan/DirectX backends. Tooling around the asset and ECS layer (editor). More thorough C++ scripting API, and potentially a different scripting language.

---

*Still highly experimental — APIs will change.*
