#ifndef MODEL_LOADER_H
#define MODEL_LOADER_H



#include <stdbool.h>
#include <stdint.h>
#include "asset_manager.h"





// Supported 3D model formats
typedef enum ModelFormat
{
    MODEL_FORMAT_UNKNOWN = 0,
    MODEL_FORMAT_OBJ,
    MODEL_FORMAT_FBX,
    MODEL_FORMAT_STL,
    MODEL_FORMAT_GLTF,
    MODEL_FORMAT_GLB,
    MODEL_FORMAT_DAE
} ModelFormat;





// Configuration profile for each model format
typedef struct ModelFormatConfig
{
    ModelFormat format;
    const char* format_name;
    uint32_t assimp_flags;
    bool supports_skeletal_data;
    bool supports_materials;
    bool has_uvs;
} ModelFormatConfig;





// --- Detection and query functions ---

ModelFormat ModelLoader_DetectFormat(const char* filepath);
ModelFormatConfig ModelLoader_GetFormatConfig(ModelFormat format);
const char* ModelLoader_GetFormatName(ModelFormat format);
bool ModelLoader_IsFormatSupported(ModelFormat format);

// Main loader function: loads and constructs a Model from disk
Model* ModelLoader_Load(AssetManager* am, const char* name, const char* filepath);





#endif // MODEL_LOADER_H