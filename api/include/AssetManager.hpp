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


        // --- Loading Methods ---
        
        static Model LoadModel(const std::string& name, const std::string& filepath);
        static Mesh LoadMesh(const std::string& name, const std::string& filepath);
        static Shader LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
        static Texture LoadTexture(const std::string& name, const std::string& filepath);
        static Texture LoadSkyboxTexture(const std::string& name, const std::string& right, const std::string& left, const std::string& top, const std::string& bottom, const std::string& front, const std::string& back);
        static AudioClip LoadAudio(const std::string& filepath);


        // --- Texture Creation ---

        static Texture CreateSolidColorTexture(const std::string& name, Prism::Color color);
        static Texture GetTextureByName(std::string name);



        // --- Material Management ---
        
        static Material CreateMaterial(Shader shader, Texture diffuse);


        // --- Built-In Assets ---
        
        static Mesh GetBuiltinQuad();
        static Mesh GetBuiltinCube();
        static Mesh GetBuiltinSphere();
        
        static Texture GetDefaultTexture();
        static Shader GetDefaultShader();
        static Shader GetDefaultSkyboxShader();
    };
}