#pragma once

#include <string>
#include <cstdint>
#include "core/Mesh.hpp"



namespace Prism
{
    // --- Replacements for core handles ---

    struct MeshHandle { uint32_t id; };
    struct TextureHandle { uint32_t id; };
    struct ShaderHandle { uint32_t id; };
    struct MaterialHandle { uint32_t id; };


    // Structure for material properties
    struct MaterialProperties
    {
        Prism::Vector3 tint_color;
        float shininess;
        float specular_strength;
    };


    // Structure for material information
    struct Material
    {
        uint32_t id;
        bool active;
        
        uint32_t shader_id;
        uint32_t diffuse_texture_id;

        MaterialProperties properties;
    };



    // ==========================================
    // Asset References
    // ==========================================

    // Reference to a specific Mesh
    struct MeshRef
    {
        Prism::MeshHandle handle;
        Prism::MeshData* data;
    };


    // Reference to a specific Material
    struct MaterialRef
    {
        Prism::MaterialHandle handle;
        Prism::Material* data;
        
        // Allows access to material properties directly
        Prism::Material* operator->() {
            return data;
        }
    };



    // ==========================================
    // Asset Manager Static Class
    // ==========================================
    
    class AssetManager
    {
    public:
        AssetManager() = delete;


        // --- LOADING METHODS ---
        
        static MeshRef LoadMesh(const std::string& name, const std::string& filepath);
        static ShaderHandle LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
        static TextureHandle LoadTexture(const std::string& name, const std::string& filepath);


        // --- MATERIAL MANAGEMENT ---
        
        static MaterialRef CreateMaterial(ShaderHandle shader, TextureHandle diffuse);
        static Material* GetMaterial(MaterialHandle handle);


        // --- BUILT-IN ASSETS ---
        
        static MeshRef GetBuiltinQuad();
        static MeshRef GetBuiltinCube();
        static MeshRef GetBuiltinSphere();
        
        static TextureHandle GetDefaultTexture();
        static ShaderHandle GetDefaultShader();

        
        // --- UTILITY ---
        
        static Prism::MeshData* GetMeshData(MeshHandle handle);
    };
}