#pragma once

#include <string>
#include <cstdint>
#include "Audio.hpp"
#include "core/Mesh.hpp"
#include "PrismAPI.hpp"



namespace Prism
{

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