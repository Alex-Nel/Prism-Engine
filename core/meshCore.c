#include "meshCore.h"
#include <stdlib.h>
#include <string.h>




// Creates a mesh from a list of vertices and indices
MeshData Mesh_Create(const Vertex3D* vertices, uint32_t v_count, const uint32_t* indices,  uint32_t i_count)
{
    MeshData data = {0};
    
    // Returns empty struct if any of the args are invalid
    if (!vertices || !indices || v_count == 0 || i_count == 0)
    {
        return data;
    }

    data.vertex_count = v_count;
    data.index_count = i_count;

    // Allocate memory
    data.vertices = (Vertex3D*)malloc(sizeof(Vertex3D) * v_count);
    data.indices  = (uint32_t*)malloc(sizeof(uint32_t) * i_count);

    if (data.vertices && data.indices)
    {
        memcpy(data.vertices, vertices, sizeof(Vertex3D) * v_count);
        memcpy(data.indices,  indices,  sizeof(uint32_t) * i_count);
    }
    else
    {
        Mesh_FreeData(&data);
    }

    return data;
}





// Free a meshes data
void Mesh_FreeData(MeshData* mesh_data)
{
    if (!mesh_data) return;
    if (mesh_data->vertices) free(mesh_data->vertices);
    if (mesh_data->indices) free(mesh_data->indices);
    
    mesh_data->vertices = NULL;
    mesh_data->indices = NULL;
    mesh_data->vertex_count = 0;
    mesh_data->index_count = 0;
}





MeshData Mesh_CreateQuad(void)
{
    // 4 corners, 2 triangles (6 indices)
    Vertex3D vertices[] = {
        {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // Bottom-Left
        {{ 0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // Bottom-Right
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Top-Right
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}  // Top-Left
    };

    uint32_t indices[] = {
        0, 1, 2, // Triangle 1
        2, 3, 0  // Triangle 2
    };

    // We reuse the dynamic function you already wrote!
    return Mesh_Create(vertices, 4, indices, 6);
}