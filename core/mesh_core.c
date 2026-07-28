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





// Calculates Tangest Vectors for a mesh based on its UV coordinates
void Mesh_CalculateVertexTangents(Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    // Initialize all tangents to zero
    for (uint32_t i = 0; i < vertex_count; i++)
        vertices[i].tangent = (Vector3){0.0f, 0.0f, 0.0f};

    
    // Loop through every triangle
    for (uint32_t i = 0; i < index_count; i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i+1];
        uint32_t i2 = indices[i+2];

        Vertex3D* v0 = &vertices[i0];
        Vertex3D* v1 = &vertices[i1];
        Vertex3D* v2 = &vertices[i2];

        // Edges of the triangle
        Vector3 edge1 = {v1->position.x - v0->position.x, v1->position.y - v0->position.y, v1->position.z - v0->position.z};
        Vector3 edge2 = {v2->position.x - v0->position.x, v2->position.y - v0->position.y, v2->position.z - v0->position.z};

        // UV Deltas
        Vector2 deltaUV1 = {v1->uv.x - v0->uv.x, v1->uv.y - v0->uv.y};
        Vector2 deltaUV2 = {v2->uv.x - v0->uv.x, v2->uv.y - v0->uv.y};

        // The Tangent Math
        float denom = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
        if (denom > -1e-6f && denom < 1e-6f)
            continue;
        float f = 1.0f / denom;

        Vector3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        // Accumulate tangents (for shared vertices)
        v0->tangent.x += tangent.x; v0->tangent.y += tangent.y; v0->tangent.z += tangent.z;
        v1->tangent.x += tangent.x; v1->tangent.y += tangent.y; v1->tangent.z += tangent.z;
        v2->tangent.x += tangent.x; v2->tangent.y += tangent.y; v2->tangent.z += tangent.z;
    }

    // Normalize all tangents at the end
    for (uint32_t i = 0; i < vertex_count; i++)
    {
        float len = sqrtf(vertices[i].tangent.x * vertices[i].tangent.x +
                          vertices[i].tangent.y * vertices[i].tangent.y +
                          vertices[i].tangent.z * vertices[i].tangent.z);
        if (len > 0.0001f)
        {
            vertices[i].tangent.x /= len;
            vertices[i].tangent.y /= len;
            vertices[i].tangent.z /= len;
        }
        else
        {
            Vector3 n = vertices[i].normal;
            Vector3 c = (fabsf(n.z) < 0.99f) ? (Vector3){ -n.y, n.x, 0.0f } : (Vector3){ 0.0f, -n.z, n.y };
            float l = sqrtf(c.x*c.x + c.y*c.y + c.z*c.z);
            vertices[i].tangent = (l > 0.0001f) ? (Vector3){ c.x/l, c.y/l, c.z/l } : (Vector3){ 1.0f, 0.0f, 0.0f };
        }
    }
}





// Calculates Tangest Vectors for a mesh based on its UV coordinates (Vertex3DSkinned)
void Mesh_CalculateVertexSkinnedTangents(Vertex3DSkinned* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    // Initialize all tangents to zero
    for (uint32_t i = 0; i < vertex_count; i++)
        vertices[i].tangent = (Vector3){0.0f, 0.0f, 0.0f};

    
    // Loop through every triangle
    for (uint32_t i = 0; i < index_count; i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i+1];
        uint32_t i2 = indices[i+2];

        Vertex3DSkinned* v0 = &vertices[i0];
        Vertex3DSkinned* v1 = &vertices[i1];
        Vertex3DSkinned* v2 = &vertices[i2];

        // Edges of the triangle
        Vector3 edge1 = {v1->position.x - v0->position.x, v1->position.y - v0->position.y, v1->position.z - v0->position.z};
        Vector3 edge2 = {v2->position.x - v0->position.x, v2->position.y - v0->position.y, v2->position.z - v0->position.z};

        // UV Deltas
        Vector2 deltaUV1 = {v1->uv.x - v0->uv.x, v1->uv.y - v0->uv.y};
        Vector2 deltaUV2 = {v2->uv.x - v0->uv.x, v2->uv.y - v0->uv.y};

        // The Tangent Math
        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        Vector3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        // Accumulate tangents (for shared vertices)
        v0->tangent.x += tangent.x; v0->tangent.y += tangent.y; v0->tangent.z += tangent.z;
        v1->tangent.x += tangent.x; v1->tangent.y += tangent.y; v1->tangent.z += tangent.z;
        v2->tangent.x += tangent.x; v2->tangent.y += tangent.y; v2->tangent.z += tangent.z;
    }

    // Normalize all tangents at the end
    for (uint32_t i = 0; i < vertex_count; i++)
        vertices[i].tangent = Vector3Normalize(vertices[i].tangent);
}





// Recalculates the bounding box for a static mesh
void Mesh_CalculateBounds(Mesh* mesh)
{
    if (!mesh || mesh->vertex_count == 0)
        return;

    mesh->local_bounds.min = (Vector3){ 999999.0f,  999999.0f,  999999.0f};
    mesh->local_bounds.max = (Vector3){-999999.0f, -999999.0f, -999999.0f};

    for (uint32_t i = 0; i < mesh->vertex_count; i++)
    {
        Vector3 pos = mesh->vertices[i].position;
        if (pos.x < mesh->local_bounds.min.x) mesh->local_bounds.min.x = pos.x;
        if (pos.y < mesh->local_bounds.min.y) mesh->local_bounds.min.y = pos.y;
        if (pos.z < mesh->local_bounds.min.z) mesh->local_bounds.min.z = pos.z;
        if (pos.x > mesh->local_bounds.max.x) mesh->local_bounds.max.x = pos.x;
        if (pos.y > mesh->local_bounds.max.y) mesh->local_bounds.max.y = pos.y;
        if (pos.z > mesh->local_bounds.max.z) mesh->local_bounds.max.z = pos.z;
    }
}





// Recalculates the normals for a static mesh
void Mesh_CalculateNormals(Vertex3D* vertices, uint32_t vertex_count, const uint32_t* indices, uint32_t index_count)
{
    // Initialize all normals to zero
    for (uint32_t i = 0; i < vertex_count; i++)
        vertices[i].normal = (Vector3){0.0f, 0.0f, 0.0f};

    // Accumulate face normals
    for (uint32_t i = 0; i < index_count; i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i+1];
        uint32_t i2 = indices[i+2];

        Vertex3D* v0 = &vertices[i0];
        Vertex3D* v1 = &vertices[i1];
        Vertex3D* v2 = &vertices[i2];

        Vector3 edge1 = {v1->position.x - v0->position.x, v1->position.y - v0->position.y, v1->position.z - v0->position.z};
        Vector3 edge2 = {v2->position.x - v0->position.x, v2->position.y - v0->position.y, v2->position.z - v0->position.z};

        // Cross product edge1 x edge2
        Vector3 normal = {
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };

        v0->normal.x += normal.x; v0->normal.y += normal.y; v0->normal.z += normal.z;
        v1->normal.x += normal.x; v1->normal.y += normal.y; v1->normal.z += normal.z;
        v2->normal.x += normal.x; v2->normal.y += normal.y; v2->normal.z += normal.z;
    }

    // Normalize them
    for (uint32_t i = 0; i < vertex_count; i++)
    {
        Vector3 n = vertices[i].normal;
        float mag = sqrtf(n.x*n.x + n.y*n.y + n.z*n.z);
        if (mag > 0.0001f)
        {
            vertices[i].normal.x /= mag;
            vertices[i].normal.y /= mag;
            vertices[i].normal.z /= mag;
        }
    }
}