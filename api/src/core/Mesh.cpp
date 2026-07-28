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



    // --- DirectionalLight ---
    
    DirectionalLight::DirectionalLight() : direction(0,-1,0), color(1,1,1), ambient_strength(0.1f) {}
    DirectionalLight::DirectionalLight(const Vector3& dir, const Color& col, float ambient) 
        : direction(dir), color(col), ambient_strength(ambient) {}



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



    // ==========================================
    // Material Implementation
    // ==========================================

    void Material::SetTintColor(const Prism::Color& color)
    {
        if (m_Handle != nullptr)
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            raw_mat->properties.albedo_tint.r = color.r;
            raw_mat->properties.albedo_tint.b = color.b;
            raw_mat->properties.albedo_tint.g = color.g;
        }
        else
        {
            Debug_Warning("Attempted to set tint color on an invalid Material");
        }
    }


    void Material::SetShininess(float shininess)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            float roughness = sqrt(2.0f / (shininess + 2.0f));
            if (roughness < 0.04f) roughness = 0.04f;
            if (roughness > 1.0f) roughness = 1.0f;
            raw_mat->properties.roughness_factor = roughness;
        }
        else
        {
            Debug_Warning("Attempted to set shininess on an invalid Material");
        }
    }

    
    void Material::SetSpecularStrength(float strength)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            float metallic = strength;
            if (metallic < 0.0f) metallic = 0.0f;
            if (metallic > 1.0f) metallic = 1.0f;
            raw_mat->properties.metallic_factor = metallic;
        }
        else
        {
            Debug_Warning("Attempted to set specular strength on an invalid Material");
        }
    }

    void Material::SetMetallic(float metallic)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (metallic < 0.0f) metallic = 0.0f;
            if (metallic > 1.0f) metallic = 1.0f;
            raw_mat->properties.metallic_factor = metallic;
        }
        else
        {
            Debug_Warning("Attempted to set metallic on an invalid Material");
        }
    }

    void Material::SetRoughness(float roughness)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (roughness < 0.0f) roughness = 0.0f;
            if (roughness > 1.0f) roughness = 1.0f;
            raw_mat->properties.roughness_factor = roughness;
        }
        else
        {
            Debug_Warning("Attempted to set roughness on an invalid Material");
        }
    }

    void Material::SetAlbedoTexture(Prism::Texture albedo)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (albedo.IsValid())
                raw_mat->albedo_texture = static_cast<::Texture*>(albedo.GetRaw());
            else
                raw_mat->albedo_texture = nullptr;
        }
        else
        {
            Debug_Warning("Attempted to set Albedo on an invalid Material");
        }
    }

    void Material::SetNormalMap(Prism::Texture normal)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (normal.IsValid())
                raw_mat->normal_map = static_cast<::Texture*>(normal.GetRaw());
            else
                raw_mat->normal_map = nullptr;
        }
        else
        {
            Debug_Warning("Attempted to set Normal Map on an invalid Material");
        }
    }

    void Material::SetMetallicMap(Prism::Texture metallic)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (metallic.IsValid())
                raw_mat->metallic_map = static_cast<::Texture*>(metallic.GetRaw());
            else
                raw_mat->metallic_map = nullptr;
        }
        else
        {
            Debug_Warning("Attempted to set Metallic Map on an invalid Material");
        }
    }

    void Material::SetRoughnessMap(Prism::Texture roughness)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (roughness.IsValid())
                raw_mat->roughness_map = static_cast<::Texture*>(roughness.GetRaw());
            else
                raw_mat->roughness_map = nullptr;
        }
        else
        {
            Debug_Warning("Attempted to set Roughness Map on an invalid Material");
        }
    }

    void Material::SetAOMap(Prism::Texture ao)
    {
        if (m_Handle != nullptr) 
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (ao.IsValid())
                raw_mat->ao_map = static_cast<::Texture*>(ao.GetRaw());
            else
                raw_mat->ao_map = nullptr;
        }
        else
        {
            Debug_Warning("Attempted to set AO Map on an invalid Material");
        }
    }

    void Material::SetShader(Prism::Shader shader)
    {
        if (m_Handle != nullptr) 
        {
            ::Shader* raw_shader = static_cast<::Shader*>(shader.GetRaw());
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            raw_mat->shader = raw_shader;
        }
        else
        {
            Debug_Warning("Attempted to set specular strength on an invalid Material");
        }
    }
}