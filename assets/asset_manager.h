#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H

#include "render/render.h"

// Initialize the registry arrays
void Asset_Init(void);

// Loads mesh from disk
MeshHandle Asset_LoadMesh(const char* name, const char* filepath);

// Loads a shader from disk, compiles it, and caches it. 
// If it was already loaded, it just returns the cached handle!
ShaderHandle Asset_LoadShader(const char* name, const char* vert_path, const char* frag_path);

// Loads a textures from disk and caches it
TextureHandle Asset_LoadTexture(const char* name, const char* filepath);

// A helper to quickly grab a standard Quad without copy-pasting code
MeshHandle Asset_GetBuiltinQuad();

// Function to get a default texture
TextureHandle Asset_GetDefaultTexture();

// In the future, you'll add things like:
// TextureHandle Asset_LoadTexture(const char* filepath);
// MeshHandle Asset_LoadOBJ(const char* filepath);

#endif