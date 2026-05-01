#include "../../include/core/Mesh.hpp"


extern "C"
{
    #include "../../../core/meshCore.h"
}


namespace Prism
{
    // --- Vertex3D ---

    Vertex3D::Vertex3D() : position(0,0,0), normal(0,0,0), uv(0,0) {}
    Vertex3D::Vertex3D(const Vector3& pos, const Vector3& norm, const Vector2& tex) 
        : position(pos), normal(norm), uv(tex) {}



    // --- AABB (Bounding Box) ---
    
    AABB::AABB() : min(0,0,0), max(0,0,0) {}
    AABB::AABB(const Vector3& min_extents, const Vector3& max_extents) 
        : min(min_extents), max(max_extents) {}



    // --- DirectionalLight ---
    
    DirectionalLight::DirectionalLight() : direction(0,-1,0), color(1,1,1), ambient_strength(0.1f) {}
    DirectionalLight::DirectionalLight(const Vector3& dir, const Vector3& col, float ambient) 
        : direction(dir), color(col), ambient_strength(ambient) {}



    // --- MeshData ---
    
    MeshData::MeshData() : vertices(nullptr), indices(nullptr), vertex_count(0), index_count(0) {}

    void MeshData::Free()
    {
        // Map to C struct for destruction
        ::MeshData raw = { 
            raw.vertices = reinterpret_cast<::Vertex3D*>(vertices), 
            raw.vertex_count = vertex_count,
            raw.indices = indices,
            raw.index_count = index_count, 
            raw.local_bounds.min = {local_bounds.min.x, local_bounds.min.y, local_bounds.min.z},
            raw.local_bounds.max = {local_bounds.max.x, local_bounds.max.y, local_bounds.max.z}
        };
        ::Mesh_FreeData(&raw);
        vertices = nullptr;
        indices = nullptr;
    }

    MeshData MeshData::Create(const Vertex3D* v, uint32_t v_count, const uint32_t* i, uint32_t i_count) {
        // Because memory layout matches, reinterpret_cast is 100% safe and fast here
        ::MeshData raw = ::Mesh_Create(reinterpret_cast<const ::Vertex3D*>(v), v_count, i, i_count);
        
        MeshData result;
        result.vertices = reinterpret_cast<Vertex3D*>(raw.vertices);
        result.indices = raw.indices;
        result.vertex_count = raw.vertex_count;
        result.index_count = raw.index_count;
        result.local_bounds = AABB(Vector3(raw.local_bounds.min.x, raw.local_bounds.min.y, raw.local_bounds.min.z),
                                   Vector3(raw.local_bounds.max.x, raw.local_bounds.max.y, raw.local_bounds.max.z));
        return result;
    }
}