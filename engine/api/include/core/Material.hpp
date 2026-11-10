#pragma once

#include "../PrismAPI.hpp"
#include "Color.hpp"



namespace Prism
{

	// ==========================================
    // EnvironmentMap Wrapper
    // ==========================================

    class PRISM_API EnvironmentMap 
    {
    private:
        void* m_Handle;

    public:
        EnvironmentMap(void* raw_env = nullptr) : m_Handle(raw_env) {}

        void* GetRaw() const {
            return m_Handle;
        }

        bool IsValid() const {
            return m_Handle != nullptr;
        }
    };





	// ==========================================
    // Texture Wrapper
    // ==========================================

    class PRISM_API Texture 
    {
    private:
        void* m_Handle;

    public:
        Texture(void* raw_tex = nullptr) : m_Handle(raw_tex) {}

        void* GetRaw() const {
            return m_Handle;
        }

        bool IsValid() const {
            return m_Handle != nullptr;
        }
    };





    // ==========================================
    // Shader Wrapper
    // ==========================================

    class PRISM_API Shader 
    {
    private:
        void* m_Handle;

    public:
        Shader(void* raw_shader = nullptr) : m_Handle(raw_shader) {}

        void* GetRaw() const {
            return m_Handle;
        }

        bool IsValid() const {
            return m_Handle != nullptr;
        }
    };





    // ==========================================
    // Material Wrapper
    // ==========================================

    class PRISM_API Material 
    {
    private:
        void* m_Handle;

    public:
        Material(void* raw_mat = nullptr) : m_Handle(raw_mat) {}

        void* GetRaw() const {
            return m_Handle;
        }

        bool IsValid() const {
            return m_Handle != nullptr;
        }

        // --- Setters for Material Properties ---
        void SetTintColor(const Prism::Color& color);
        void SetShininess(float shininess);
        void SetSpecularStrength(float strength);
        void SetMetallic(float metallic);
        void SetRoughness(float roughness);
        void SetAlbedoTexture(Prism::Texture albedo);
        void SetNormalMap(Prism::Texture normal);
        void SetMetallicMap(Prism::Texture metallic);
        void SetRoughnessMap(Prism::Texture roughness);
        void SetAOMap(Prism::Texture ao);
        void SetShader(Prism::Shader shader);
    };

}