#include "../../include/core/Material.hpp"
#include "../../include/core/Log.hpp"


extern "C"
{
    #include "../../../core/mesh_core.h"
}



namespace Prism
{

	// ==========================================
    // Material Implementation
    // ==========================================

    void Material::SetTintColor(const Prism::Color& color) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            raw_mat->properties.albedo_tint.r = color.r;
            raw_mat->properties.albedo_tint.b = color.b;
            raw_mat->properties.albedo_tint.g = color.g;
        }
        else {
            Debug_Warning("Attempted to set tint color on an invalid Material");
        }
    }


    void Material::SetShininess(float shininess) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            float roughness = sqrt(2.0f / (shininess + 2.0f));
            if (roughness < 0.04f) roughness = 0.04f;
            if (roughness > 1.0f) roughness = 1.0f;
            raw_mat->properties.roughness_factor = roughness;
        }
        else {
            Debug_Warning("Attempted to set shininess on an invalid Material");
        }
    }

    
    void Material::SetSpecularStrength(float strength) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            float metallic = strength;
            if (metallic < 0.0f) metallic = 0.0f;
            if (metallic > 1.0f) metallic = 1.0f;
            raw_mat->properties.metallic_factor = metallic;
        }
        else {
            Debug_Warning("Attempted to set specular strength on an invalid Material");
        }
    }


    void Material::SetMetallic(float metallic) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (metallic < 0.0f) metallic = 0.0f;
            if (metallic > 1.0f) metallic = 1.0f;
            raw_mat->properties.metallic_factor = metallic;
        }
        else {
            Debug_Warning("Attempted to set metallic on an invalid Material");
        }
    }


    void Material::SetRoughness(float roughness) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (roughness < 0.0f) roughness = 0.0f;
            if (roughness > 1.0f) roughness = 1.0f;
            raw_mat->properties.roughness_factor = roughness;
        }
        else {
            Debug_Warning("Attempted to set roughness on an invalid Material");
        }
    }


    void Material::SetAlbedoTexture(Prism::Texture albedo) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (albedo.IsValid())
                raw_mat->albedo_texture = static_cast<::Texture*>(albedo.GetRaw());
            else
                raw_mat->albedo_texture = nullptr;
        }
        else {
            Debug_Warning("Attempted to set Albedo on an invalid Material");
        }
    }


    void Material::SetNormalMap(Prism::Texture normal) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (normal.IsValid())
                raw_mat->normal_map = static_cast<::Texture*>(normal.GetRaw());
            else
                raw_mat->normal_map = nullptr;
        }
        else {
            Debug_Warning("Attempted to set Normal Map on an invalid Material");
        }
    }


    void Material::SetMetallicMap(Prism::Texture metallic) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (metallic.IsValid())
                raw_mat->metallic_map = static_cast<::Texture*>(metallic.GetRaw());
            else
                raw_mat->metallic_map = nullptr;
        }
        else {
            Debug_Warning("Attempted to set Metallic Map on an invalid Material");
        }
    }


    void Material::SetRoughnessMap(Prism::Texture roughness) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (roughness.IsValid())
                raw_mat->roughness_map = static_cast<::Texture*>(roughness.GetRaw());
            else
                raw_mat->roughness_map = nullptr;
        }
        else {
            Debug_Warning("Attempted to set Roughness Map on an invalid Material");
        }
    }


    void Material::SetAOMap(Prism::Texture ao) {
        if (m_Handle != nullptr) {
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            if (ao.IsValid())
                raw_mat->ao_map = static_cast<::Texture*>(ao.GetRaw());
            else
                raw_mat->ao_map = nullptr;
        }
        else {
            Debug_Warning("Attempted to set AO Map on an invalid Material");
        }
    }


    void Material::SetShader(Prism::Shader shader) {
        if (m_Handle != nullptr) {
            ::Shader* raw_shader = static_cast<::Shader*>(shader.GetRaw());
            ::Material* raw_mat = static_cast<::Material*>(m_Handle);
            raw_mat->shader = raw_shader;
        }
        else {
            Debug_Warning("Attempted to set specular strength on an invalid Material");
        }
    }

}