#pragma once

extern "C"
{
    #include "core/meshCore.h"
}

#include "Math.hpp"

namespace Prism
{
    // ==========================================
    // Vertex3D Wrapper
    // ==========================================
    struct Vertex3D : public ::Vertex3D
    {
        // --- Constructors ---    
        Vertex3D() {
            position = {0, 0, 0};
            normal = {0, 0, 0};
            uv = {0, 0};
        }

        // We can use our Prism::Vector classes here for clean syntax
        Vertex3D(const Prism::Vector3& pos, const Prism::Vector3& norm, const Prism::Vector2& tex) {
            position = pos;
            normal = norm;
            uv = tex;
        }

        Vertex3D(const ::Vertex3D& raw) { 
            position = raw.position;
            normal = raw.normal;
            uv = raw.uv;
        }

        // operator ::Vertex3D() const { return *this; }
    };


    // ==========================================
    // AABB (BOUNDING BOX) Wrapper
    // ==========================================
    struct AABB : public ::AABB
    {    
        AABB() {
            min = {0, 0, 0};
            max = {0, 0, 0};
        }

        AABB(const Prism::Vector3& min_extents, const Prism::Vector3& max_extents) {
            min = min_extents;
            max = max_extents;
        }

        AABB(const ::AABB& raw) {
            min = raw.min;
            max = raw.max;
        }

        // operator ::AABB() const { return *this; }
    };


    // ==========================================
    // DIRECTIONAL LIGHT WRAPPER
    // ==========================================
    struct DirectionalLight : public ::DirectionalLight
    {    
        DirectionalLight() {
            direction = {0, -1, 0};
            color = {1, 1, 1};
            ambient_strength = 0.1f;
        }

        DirectionalLight(const Prism::Vector3& dir, const Prism::Vector3& col, float ambient) {
            direction = dir;
            color = col;
            ambient_strength = ambient;
        }

        DirectionalLight(const ::DirectionalLight& raw) {
            direction = raw.direction;
            color = raw.color;
            ambient_strength = raw.ambient_strength;
        }

        // operator ::DirectionalLight() const { return *this; }
    };


    // ==========================================
    // MESH DATA WRAPPER
    // ==========================================
    struct MeshData : public ::MeshData
    {    
        // Default empty constructor
        MeshData() {
            vertices = nullptr;
            indices = nullptr;
            vertex_count = 0;
            index_count = 0;
        }

        // Implicit conversions
        MeshData(const ::MeshData& raw) {
            vertices = raw.vertices;
            vertex_count = raw.vertex_count;
            indices = raw.indices;
            index_count = raw.index_count;
            local_bounds = raw.local_bounds;
        }

        // operator ::MeshData() const { return *this; }


        // --- Memory Management Methods ---
        
        // Frees the underlying arrays
        void Free() {
            Mesh_FreeData(this);
        }


        // --- Class Static Methods ---
        
        // Creates a new mesh from provided arrays
        static MeshData Create(const Prism::Vertex3D* v, uint32_t v_count, const uint32_t* i, uint32_t i_count) {
            // Prism::Vertex3D has the same memory layout as ::Vertex3D, so just cast to C type
            return Mesh_Create(reinterpret_cast<const ::Vertex3D*>(v), v_count, i, i_count);
        }
    };
}