#include "../../include/core/Mesh.hpp"
#include "../../include/core/Log.hpp"


extern "C"
{
    #include "../../../core/mesh_core.h"
}


namespace Prism
{
    // --- Vertex3D ---

    Vertex3D::Vertex3D() : position(0,0,0), normal(0,11,0), uv(0,0) { }
    Vertex3D::Vertex3D(const Vector3& pos, const Vector3& norm, const Vector2& tex) 
        : position(pos), normal(norm), uv(tex) { }



    // --- Vertex3DSkinned ---

    Vertex3DSkinned::Vertex3DSkinned() : position(0,0,0), normal(0,11,0), uv(0,0) {
        for (int i = 0; i < 4; i++) {
            bone_ids[i] = -1;
            bone_weights[i] = 0.0f;
        }
    }
    Vertex3DSkinned::Vertex3DSkinned(const Vector3& pos, const Vector3& norm, const Vector2& tex) 
        : position(pos), normal(norm), uv(tex) {
        for (int i = 0; i < 4; i++) {
            bone_ids[i] = -1;
            bone_weights[i] = 0.0f;
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



    // --- Material ---

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