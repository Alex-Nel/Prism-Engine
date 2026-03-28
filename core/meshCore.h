#ifndef MESH_CORE_H
#define MESH_CORE_H

#include <stdint.h>

#include "mathCore.h"



// Vertex3D is purely geometric data.
typedef struct Vertex3D
{
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
} Vertex3D;

// The CPU-side container
typedef struct MeshData
{
    Vertex3D* vertices;
    uint32_t vertex_count;
    
    uint32_t* indices;
    uint32_t index_count;
} MeshData;

// struct for a global scene light
typedef struct DirectionalLight
{
    Vector3 position;
    Vector3 color;
    float ambient_strength;
} DirectionalLight;





// --- API ---

// Creates a mesh from a user-specified list of vertices and indices
MeshData Mesh_Create(const Vertex3D* vertices, uint32_t v_count, const uint32_t* indices,  uint32_t i_count);

// Frees the CPU memory
void Mesh_FreeData(MeshData* mesh_data);

// Generates a generic quad
MeshData Mesh_CreateQuad(void);



#endif // CORE_MESH_H