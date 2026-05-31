#pragma once

#include <string>
#include <cstdint>
#include "Audio.hpp"
#include "core/Mesh.hpp"
#include "PrismAPI.hpp"



namespace Prism
{
    // --- Replacements for core handles ---

    // struct PRISM_API MeshHandle { uint32_t id; };
    // struct PRISM_API TextureHandle { uint32_t id; };
    // struct PRISM_API ShaderHandle { uint32_t id; };
    // struct PRISM_API MaterialHandle { uint32_t id; };


    // // Structure for material properties
    // struct PRISM_API MaterialProperties
    // {
    //     Prism::Vector3 tint_color;
    //     float shininess;
    //     float specular_strength;
    // };


    // // Structure for material information
    // struct PRISM_API Material
    // {
    //     uint32_t id;
    //     bool active;
        
    //     uint32_t shader_id;
    //     uint32_t diffuse_texture_id;

    //     MaterialProperties properties;
    // };



    // ==========================================
    // Asset Manager Static Class
    // ==========================================
    
    class PRISM_API AssetManager
    {
    public:
        AssetManager() = delete;


        // --- LOADING METHODS ---
        
        static Model LoadModel(const std::string& name, const std::string& filepath);
        static Mesh LoadMesh(const std::string& name, const std::string& filepath);
        static Shader LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
        static Texture LoadTexture(const std::string& name, const std::string& filepath);
        static AudioClip LoadAudio(const std::string& filepath);


        // --- MATERIAL MANAGEMENT ---
        
        static Material CreateMaterial(Shader shader, Texture diffuse);


        // --- BUILT-IN ASSETS ---
        
        static Mesh GetBuiltinQuad();
        static Mesh GetBuiltinCube();
        static Mesh GetBuiltinSphere();
        
        static Texture GetDefaultTexture();
        static Shader GetDefaultShader();
    };
}