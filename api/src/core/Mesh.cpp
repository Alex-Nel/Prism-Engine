#include "../../include/core/Mesh.hpp"
#include "../../include/core/Log.hpp"


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



    // --- Material ---

    void Material::SetTintColor(const Prism::Vector3& color)
    {
        if (m_Handle != nullptr)
        {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            raw_mat->properties.tint_color.x = color.x;
            raw_mat->properties.tint_color.y = color.y;
            raw_mat->properties.tint_color.z = color.z;
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
            raw_mat->properties.shininess = shininess;
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
            raw_mat->properties.specular_strength = strength;
        }
        else
        {
            Debug_Warning("Attempted to set specular strength on an invalid Material");
        }
    }
    
}