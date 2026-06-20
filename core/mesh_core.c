#include "mesh_core.h"
#include <stdlib.h>
#include <string.h>





// Sets a vertex to a safe, un-animated state
void Vertex_SetDefaultBones(Vertex3D* v)
{
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
    {
        v->bone_ids[i] = -1;   // -1 means "No bone affects this slot"
        v->bone_weights[i] = 0.0f; // 0 influence
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Move these functions to asset manager to be able to procedurally generate terrain or custom shapes from code later
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// TODO: Implement a procedural mesh function
// Prototype:
// asset_manager.c
// Mesh* Asset_CreateProceduralMesh(const char* name, const Vertex3D* vertices, uint32_t v_count, const uint32_t* indices, uint32_t i_count)
// {
//     if (mesh_count >= MAX_CACHED_MESHES) return NULL;

//     // Send to GPU
//     MeshHandle gpu = Render_CreateMesh(renderer, vertices, v_count, indices, i_count);

//     // Cache it
//     Mesh* m = &mesh_cache[mesh_count];
//     strcpy(m->name, name);
//     m->id = mesh_count;
//     m->gpu_handle = gpu;
    
//     // Allocate heap memory and copy the data so the engine owns it
//     m->vertices = malloc(sizeof(Vertex3D) * v_count);
//     m->indices = malloc(sizeof(uint32_t) * i_count);
//     memcpy(m->vertices, vertices, sizeof(Vertex3D) * v_count);
//     memcpy(m->indices, indices, sizeof(uint32_t) * i_count);
    
//     m->vertex_count = v_count;
//     m->index_count = i_count;
//     m->local_bounds = CalculateAABB(m->vertices, v_count);

//     mesh_count++;
//     return m;
// }