#ifndef MESH_CORE_H
#define MESH_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "mathCore.h"



// Vertex3D struct for geometric data.
typedef struct Vertex3D
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
} Vertex3D;



// Struct for a global scene light
typedef struct DirectionalLight
{
    Vector3 direction;
    Vector3 color;
    float ambient_strength;
} DirectionalLight;



// Struct for an Axis Aligned Bounding Box
typedef struct AABB
{
    Vector3 min;
    Vector3 max;
} AABB;



// Struct for all mesh data
typedef struct MeshData
{
    Vertex3D* vertices;
    uint32_t vertex_count;
    
    uint32_t* indices;
    uint32_t index_count;

    AABB local_bounds;
} MeshData;





// Creates a mesh from a user-specified list of vertices and indices
MeshData Mesh_Create(const Vertex3D* vertices, uint32_t v_count, const uint32_t* indices,  uint32_t i_count);

// Frees a mesh's memory
void Mesh_FreeData(MeshData* mesh_data);



#endif