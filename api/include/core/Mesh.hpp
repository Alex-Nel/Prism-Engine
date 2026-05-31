#pragma once

#include "Math.hpp"
#include <cstdint>
#include "../PrismAPI.hpp"



struct Model;
struct Texture;
struct Shader;
struct Material;
struct Model;


namespace Prism
{
    // ==========================================
    // Vertex3D Wrapper
    // ==========================================

    struct PRISM_API Vertex3D
    {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;


        // --- Constructors ---   

        Vertex3D();
        Vertex3D(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex);
    };



    // ==========================================
    // AABB (BOUNDING BOX) Wrapper
    // ==========================================
    
    struct PRISM_API AABB
    {    
        Vector3 min;
        Vector3 max;


        // --- Constructors ---  

        AABB();
        AABB(const Prism::Vector3& min_extents, const Prism::Vector3& max_extents);
    };



    // ==========================================
    // Directional Light Wrapper
    // ==========================================

    struct PRISM_API DirectionalLight
    {    
        Vector3 direction;
        Vector3 color;
        float ambient_strength;


        // --- Constructors ---  

        DirectionalLight();
        DirectionalLight(const Vector3& dir, const Vector3& col, float ambient);
    };



    // ==========================================
    // Mesh Wrapper
    // ==========================================

    class PRISM_API Mesh
    {    
    private:
        void* m_Handle;
        
    public:
        Mesh(void* raw_mesh = nullptr) : m_Handle(raw_mesh) {}

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
        void SetTintColor(const Prism::Vector3& color);
        void SetShininess(float shininess);
        void SetSpecularStrength(float strength);
    };



    // ==========================================
    // Model Wrapper
    // ==========================================

    class PRISM_API Model 
    {
    private:
        void* m_Handle;

    public:
        Model(void* raw_model = nullptr) : m_Handle(raw_model) {}

        void* GetRawModel() const {
            return m_Handle;
        }

        bool IsValid() const {
            return m_Handle != nullptr;
        }
    };
}