#pragma once

#include <string>
#include <cstdint>
#include "Audio.hpp"
#include "core/Mesh.hpp"
#include "PrismAPI.hpp"



namespace Prism
{

    // ==========================================
    // Model Wrapper
    // ==========================================

    class PRISM_API Model 
    {
    private:
        void* m_Handle;

    public:
        Model(void* raw_model = nullptr) : m_Handle(raw_model) {}

        void* GetRawModel() const { return m_Handle; }
        bool IsValid() const { return m_Handle != nullptr; }

        void* GetRawSkeleton() const;
        uint32_t GetAnimationCount() const;
        Prism::AnimationClip GetAnimation(uint32_t index) const;
        Prism::AnimationClip GetAnimation(const std::string& name) const;
        Prism::Material GetMaterial(uint32_t mesh_index) const;
        uint32_t GetMeshCount() const;
        bool IsMeshSkinned(uint32_t index) const;
        Prism::Mesh GetMesh(uint32_t index) const;
        Prism::SkinnedMesh GetSkinnedMesh(uint32_t index) const;
    };





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
        static AudioClip LoadAudio(const std::string& filepath);

        static Model GetModelByName(const std::string& name);
        static Mesh GetMeshByName(const std::string& name);
        static SkinnedMesh GetSkinnedMeshByName(const std::string& name);


        // --- Texture Management ---

        static Texture LoadTexture(const std::string& name, const std::string& filepath);
        static Texture LoadSkyboxTexture(const std::string& name, const std::string& right, const std::string& left, const std::string& top, const std::string& bottom, const std::string& front, const std::string& back);
        static EnvironmentMap LoadEnvironmentMap(const std::string& filepath);
        static EnvironmentMap LoadEnvironmentMapFromSkybox(const std::string& name, const std::string& right, const std::string& left, const std::string& top, const std::string& bottom, const std::string& front, const std::string& back);
        static Texture CreateSolidColorTexture(const std::string& name, Prism::Color color);
        static Texture GetTextureByName(std::string name);


        // --- Dynamic Meshes ---

        static Mesh CreateDynamicMesh(uint32_t max_vertices, uint32_t max_indices);
        static void UpdateDynamicMesh(Mesh mesh, Prism::Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count);


        // --- Material Management ---
        
        static Material CreateMaterial(Texture diffuse);


        // --- Built-In Assets ---
        
        static Mesh GetBuiltinQuad();
        static Mesh GetBuiltinCube();
        static Mesh GetBuiltinSphere();
        
        static Texture GetDefaultTexture();
    };
}