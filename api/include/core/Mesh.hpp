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
    // Vertex3DSkinned Wrapper
    // ==========================================

    struct PRISM_API Vertex3DSkinned
    {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;

        int bone_ids[4];
        float bone_weights[4];


        // --- Constructors ---   

        Vertex3DSkinned();
        Vertex3DSkinned(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex);
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
    // Color Wrapper
    // ==========================================

    struct PRISM_API Color
    {
        float r, g, b, a;

        // --- Constructors --- 

        Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
        Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
        

        // --- static preset colors ---
        
        static Color White()   { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Black()   { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color Red()     { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static Color Green()   { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static Color Blue()    { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static Color Clear()   { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
    };



    // ==========================================
    // Directional Light Wrapper
    // ==========================================

    struct PRISM_API DirectionalLight
    {    
        Vector3 direction;
        Color color;
        float ambient_strength;


        // --- Constructors ---  

        DirectionalLight();
        DirectionalLight(const Vector3& dir, const Color& col, float ambient);
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

        void* GetRaw() const { return m_Handle; }
        bool IsValid() const { return m_Handle != nullptr; }
    };



    // ==========================================
    // Skinned Mesh Wrapper
    // ==========================================

    class PRISM_API SkinnedMesh
    {    
    private:
        void* m_Handle;
        
    public:
        SkinnedMesh(void* raw_mesh = nullptr) : m_Handle(raw_mesh) {}
        
        void* GetRaw() const { return m_Handle; }
        bool IsValid() const { return m_Handle != nullptr; }
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
    };



    // ==========================================
    // AnimationClip Wrapper
    // ==========================================

    class PRISM_API AnimationClip
    {
    private:
        void* m_RawClip;
        
    public:
        AnimationClip(void* raw_clip) : m_RawClip(raw_clip) {}

        void* GetRaw() const {
            return m_RawClip;
        }

        bool IsValid() const {
            return m_RawClip != nullptr;
        }
    };
}