#include "mesh_core.h"
#include <stdlib.h>
#include <string.h>





// Computes the AABB for a skinned meshes current pose
AABB SkinnedMesh_ComputePoseAABB(const SkinnedMesh* mesh, const Matrix4* bone_matrices)
{
    AABB bounds = {
        (Vector3){ FLT_MAX,  FLT_MAX,  FLT_MAX},
        (Vector3){-FLT_MAX, -FLT_MAX, -FLT_MAX}
    };

    if (!mesh || !bone_matrices || mesh->vertex_count == 0)
        return mesh ? mesh->local_bounds : bounds;

    for (uint32_t i = 0; i < mesh->vertex_count; i++)
    {
        const Vertex3DSkinned* v = &mesh->vertices[i];
        Vector3 skinned_pos = {0.0f, 0.0f, 0.0f};
        float total_weight = 0.0f;

        for (int b = 0; b < MAX_BONE_INFLUENCE; b++)
        {
            if (v->bone_ids[b] == -1)
                continue;

            float w = v->bone_weights[b];
            Vector3 transformed = Matrix4MultiplyVector3(bone_matrices[v->bone_ids[b]], v->position);
            skinned_pos.x += transformed.x * w;
            skinned_pos.y += transformed.y * w;
            skinned_pos.z += transformed.z * w;
            total_weight += w;
        }

        if (total_weight <= 0.0001f)
            skinned_pos = v->position;

        if (skinned_pos.x < bounds.min.x) bounds.min.x = skinned_pos.x;
        if (skinned_pos.y < bounds.min.y) bounds.min.y = skinned_pos.y;
        if (skinned_pos.z < bounds.min.z) bounds.min.z = skinned_pos.z;
        if (skinned_pos.x > bounds.max.x) bounds.max.x = skinned_pos.x;
        if (skinned_pos.y > bounds.max.y) bounds.max.y = skinned_pos.y;
        if (skinned_pos.z > bounds.max.z) bounds.max.z = skinned_pos.z;
    }

    return bounds;
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