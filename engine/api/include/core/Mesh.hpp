#pragma once

#include "Math.hpp"
#include "Color.hpp"
#include <cstdint>
#include <vector>
#include "../PrismAPI.hpp"





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
        Vector3 tangent;


        // --- Constructors ---   

        Vertex3D();
        Vertex3D(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex, const Prism::Vector3& tan = Prism::Vector3(0,0,0));
    };





    // ==========================================
    // Vertex3DSkinned Wrapper
    // ==========================================

    struct PRISM_API Vertex3DSkinned
    {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;
        Vector3 tangent;

        int bone_ids[4];
        float bone_weights[4];


        // --- Constructors ---   

        Vertex3DSkinned();
        Vertex3DSkinned(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex, const Prism::Vector3& tan = Prism::Vector3(0,0,0));
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

        std::vector<Prism::Vertex3D> GetVertices() const;
        std::vector<uint32_t> GetIndices() const;
        void SetVertices(const std::vector<Prism::Vertex3D>& vertices, const std::vector<uint32_t>& indices);

        void RecalculateBounds();
        void RecalculateNormals();
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