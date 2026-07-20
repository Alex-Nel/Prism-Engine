# Prism Engine

A 3D engine aimed at being minimal, yet comprehensive for real projects. It includes a **data-oriented Entity Component System**, API agnostic renderer, **SDL3** support for windowing and input, **box3d** physics for rigid bodies, raycasts, and collision callbacks, **Open Asset Importer** for asset management, and cJSON for serialization.

---

## Features

| Feature | Details |
|------|----------------|
| **Scene system** | Custom Entity Component System with several built in components. Parent/child hierarchy for transforms; Support for up to **32,768** entities |
| **Rendering backends** | API-agnostic rendering layer with customizable settings (SSAO, Gamma, etc.). Utilizes a Deferred Renderer. Support for **OpenGL** and Headless mode. |
| **Materials & meshes** | Supports Physically Based Rendering with custom shaders and material properties. Built in default meshes (quad / cube / sphere). Custom meshes/models using **Open Asset Importer**, with support for multiple file types. |
| **Physics** | **Box3d** physics integration. Box, sphere, mesh, and convex colliders and triggers. Support for collision layers & masks, rigidbodies, single & multi raycasts, and custom physics callbacks. |
| **Scripting** | Easy to use C++ scripting API in an OOP design. |

---

## Project structure

```
Prism Engine/
├── api/                       # Headers and implementation for the C++ API
├── assets/                    # Asset manager (models, materials, textures, and shaders)
├── audio/                     # Audio Management and miniaudio integration
├── core/                      # Core utilities: Math, mesh, input, events, time, log, I/O
├── include/                   # Third-party headers - GLAD, miniaudio, stb_image
├── platform/                  # Window and OS API declaration and usage using SDL3
├── render/                    # Render interface and backend implementation (Currently OpenGL and Headless only)
├── scene/                     # Scene system, ECS, physics integration
└── Prism.c / Prism.h          # Engine lifecycle. Ties all modules.
```

---

## Build Instructions

**Toolchain:**

- Linux & MacOS: **GCC**, **G++**, and CMake.
- Windows: **MinGW-w64** (recommended), and CMake.



```bash
mkdir build                  - Create a build folder
cmake -S . -B build          - Generate build files ( additionally add -DCMAKE_BUILD_TYPE=Debug to do debug builds )
cmake --build build          - Start build ( static and dynamic library options are available )
```
(CMake should automatically fetch SDL3, Box3d, cJSON, and AssImp if you don't have it.)

Result: **`libPrismEngine.dll`** (or **`libPrismEngine.so`** on Linux), and **`libPrismEngine.a`** for building static.


## Usage (minimal)

Prerequisites:
1. All C++ Headers are in **`api/include`**. **`api/include/Prism.hpp`** includes all other headers.
2. Link with Prism lib file (.dll/.so). If linking statically, you'll need to link with SDL3, Box3d, AssImp, and cJSON.

Basic Usage:
1. Initialize engine with **`Prism::Engine::Init()`**
2. Initialize scene with **`Prism::Scene::Create()`**
3. Add entities, components, assets, custom scripts, etc. using respective functions.
4. Run the engine with **`Prism::Engine::Run()`**.
5. When exiting, run **`Prism::Engine::Shutdown()`**




## Running Engine

All assets used are relative to the executable's path (unless providing an absolute path). No sample textures or models provided, so add your own.

---

## Future plans

Implementing Vulkan/DirectX backends. Tooling like an editor and map maker. More thorough C++ scripting API, and potentially a different scripting language.

---

*Still highly experimental — APIs will change.*
