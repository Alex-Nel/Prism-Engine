#include "../../include/core/Mesh.hpp"
#include "../../include/core/Log.hpp"


extern "C"
{
    #include "../../../core/mesh_core.h"
    #include "../../../assets/asset_manager.h"
}


namespace Prism
{
    
    // --- Vertex3D ---

    Vertex3D::Vertex3D() {
        this->position = Prism::Vector3(0,0,0);
        this->normal = Prism::Vector3(0,1,0);
        this->uv = Prism::Vector2(0,0);
        this->tangent = Prism::Vector3(0,0,0);
    }

    Vertex3D::Vertex3D(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex, const Prism::Vector3& tan) {
        this->position = pos;
        this->normal = norm;
        this->uv = tex;
        this->tangent = tan;
    }





    // --- Vertex3DSkinned ---

    Vertex3DSkinned::Vertex3DSkinned() {
        this->position = Prism::Vector3(0,0,0);
        this->normal = Prism::Vector3(0,1,0);
        this->uv = Prism::Vector2(0,0);
        this->tangent = Prism::Vector3(0,0,0);
        for (int i = 0; i < 4; i++) {
            this->bone_ids[i] = 0; this->bone_weights[i] = 0.0f;
        }
    }

    Vertex3DSkinned::Vertex3DSkinned(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex, const Prism::Vector3& tan) {
        this->position = pos;
        this->normal = norm;
        this->uv = tex;
        this->tangent = tan;
        for (int i = 0; i < 4; i++) {
            this->bone_ids[i] = 0; this->bone_weights[i] = 0.0f;
        }
    }





    // --- AABB (Bounding Box) ---
    
    AABB::AABB() : min(0,0,0), max(0,0,0) {}
    AABB::AABB(const Vector3& min_extents, const Vector3& max_extents) 
        : min(min_extents), max(max_extents) {}





    // ==========================================
    // Mesh Implementation
    // ==========================================

    std::vector<Prism::Vertex3D> Mesh::GetVertices() const {
        if (!IsValid())
            return {};
        ::Mesh* c_mesh = static_cast<::Mesh*>(m_Handle);
        std::vector<Prism::Vertex3D> vertices(c_mesh->vertex_count);
        for (uint32_t i = 0; i < c_mesh->vertex_count; i++) {
            vertices[i] = Prism::Vertex3D(
                Prism::Vector3(c_mesh->vertices[i].position.x, c_mesh->vertices[i].position.y, c_mesh->vertices[i].position.z),
                Prism::Vector3(c_mesh->vertices[i].normal.x, c_mesh->vertices[i].normal.y, c_mesh->vertices[i].normal.z),
                Prism::Vector2(c_mesh->vertices[i].uv.x, c_mesh->vertices[i].uv.y),
                Prism::Vector3(c_mesh->vertices[i].tangent.x, c_mesh->vertices[i].tangent.y, c_mesh->vertices[i].tangent.z)
            );
        }
        return vertices;
    }

    std::vector<uint32_t> Mesh::GetIndices() const {
        if (!IsValid())
            return {};
        ::Mesh* c_mesh = static_cast<::Mesh*>(m_Handle);
        std::vector<uint32_t> indices(c_mesh->index_count);
        for (uint32_t i = 0; i < c_mesh->index_count; i++) {
            indices[i] = c_mesh->indices[i];
        }
        return indices;
    }

    void Mesh::SetVertices(const std::vector<Prism::Vertex3D>& vertices, const std::vector<uint32_t>& indices) {
        if (!IsValid())
            return;
        ::Mesh* c_mesh = static_cast<::Mesh*>(m_Handle);
        ::Asset_UpdateMesh(c_mesh, (::Vertex3D*)vertices.data(), (uint32_t)vertices.size(), (uint32_t*)indices.data(), (uint32_t)indices.size());
    }

    void Mesh::RecalculateBounds() {
        if (!IsValid())
            return;
        ::Mesh* c_mesh = static_cast<::Mesh*>(m_Handle);
        ::Mesh_CalculateBounds(c_mesh);
    }

    void Mesh::RecalculateNormals() {
        if (!IsValid())
            return;
        ::Mesh* c_mesh = static_cast<::Mesh*>(m_Handle);
        ::Mesh_CalculateNormals(c_mesh->vertices, c_mesh->vertex_count, c_mesh->indices, c_mesh->index_count);
        
        // Pushing to GPU automatically recalculates tangents inside Asset_UpdateMesh
        ::Asset_UpdateMesh(c_mesh, c_mesh->vertices, c_mesh->vertex_count, c_mesh->indices, c_mesh->index_count);
    }

}