#ifndef MATERIAL_LOADER_H
#define MATERIAL_LOADER_H


#include "asset_manager.h"


#ifdef __cplusplus
extern "C" {
#endif



// Loads a material from a .prismmat JSON file
Material* MaterialLoader_Load(AssetManager* am, const char* filepath);

// Saves a material to a .prismmat JSON file
bool MaterialLoader_Save(AssetManager* am, Material* material, const char* filepath);



#ifdef __cplusplus
}
#endif


#endif // MATERIAL_LOADER_H