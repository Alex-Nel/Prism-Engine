<h1 align="center">Prism Engine</h1>

A 3D engine aimed at being minimal, yet comprehensive for real projects. It includes a data-oriented Entity Component System, API agnostic renderer, **Box3d** physics for rigid bodies, raycasts, and collision callbacks, and **Open Asset Importer** for asset management.

---

## Features

| Feature | Details |
|------|----------------|
| **Scene system** | Custom Entity Component System with several built in components (including ones for UI overlays). Parent/child hierarchy for transforms. Support for up to **32,768** entities |
| **Rendering** | API-agnostic rendering layer with customizable settings (SSAO, Gamma, etc.). Utilizes a hybrid Deferred and Forward Renderer. Support for **OpenGL** and Headless mode. |
| **Assets** | Supports Physically-Based Rendering with custom shaders and material. Built in default meshes (quad / cube / sphere). Custom model support using **Open Asset Importer** |
| **Physics** | **Box3d** physics integration. Box, sphere, mesh, and convex colliders and triggers. Support for collision layers & masks, rigidbodies, single & multi raycasts, and custom physics callbacks. |
| **Scripting** | Easy to use C++ scripting API in an OOP design. |

---

## Repo Structure

```
Prism Engine/
├── api/                       # Headers and implementation for the C++ API
├── assets/                    # Asset manager (models, materials, textures, shaders, fonts)
├── audio/                     # Audio clip and source Management
├── core/                      # Core utilities: Events, Time, Log, I/O, Color, Math, Mesh, Input, UI
├── external/                  # Third-party headers - GLAD, miniaudio, nuklear, stb_image, stb_truetype
├── platform/                  # Windowing and OS management
├── render/                    # Renderer interface and backend implementation
├── runtime/                   # Engine runtime for initialization, main loop, and basic utilities
└── scene/                     # Scene system, Components, UI overlays, serialization, and physics integration
```

---

## Build Instructions

**Toolchain:** Any C11 and C++17 compiler (GCC, Clang, MSVC) + CMake

```bash
mkdir build
cmake -S . -B build       // add -DCMAKE_BUILD_TYPE=Debug to make a debug build
cmake --build build
```

**Result:** A .dll/.so file if building dynamically, and .a file if building statically

## Usage

Prerequisites:
1. All C++ Headers are in **`api/include`**. **`api/include/Prism.hpp`** will include everything.
2. Link with Prism lib file (.dll/.so). If linking statically, you'll also need to link with SDL3, Box3d, AssImp, and cJSON.

Basic Usage:
1. Initialize engine with **`Prism::Engine::Init()`**
2. Initialize scene with **`Prism::Scene::Create()`**
3. Add entities, components, assets, custom scripts, etc. using respective functions.
4. Run the engine with **`Prism::Engine::Run()`**.
5. When exiting, run **`Prism::Engine::Shutdown()`**


*All assets used are relative to the executable's path (unless providing an absolute path).*

---

## Future plans

- Implementing Vulkan/DirectX backends.
- Tooling like an editor and map maker.
- More thorough C++ scripting API
- Potentially a different scripting language.

---

*Still highly experimental — APIs will change.*