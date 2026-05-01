#pragma once

#include "Math.hpp"
#include <cstdint>



namespace Prism
{
    // ==========================================
    // Vertex3D Wrapper
    // ==========================================

    struct Vertex3D
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
    
    struct AABB
    {    
        Vector3 min;
        Vector3 max;


        // --- Constructors ---  

        AABB();
        AABB(const Prism::Vector3& min_extents, const Prism::Vector3& max_extents);
    };



    // ==========================================
    // DIRECTIONAL LIGHT WRAPPER
    // ==========================================

    struct DirectionalLight
    {    
        Vector3 direction;
        Vector3 color;
        float ambient_strength;


        // --- Constructors ---  

        DirectionalLight();
        DirectionalLight(const Vector3& dir, const Vector3& col, float ambient);
    };


    // ==========================================
    // MESH DATA WRAPPER
    // ==========================================

    struct MeshData
    {    
        Vertex3D* vertices;
        uint32_t* indices;
        uint32_t vertex_count;
        uint32_t index_count;
        AABB local_bounds;


        // --- Constructors ---  

        MeshData();


        // --- Mesh Functions ---

        void Free();
        static MeshData Create(const Vertex3D* v, uint32_t v_count, const uint32_t* i, uint32_t i_count);
    };
}