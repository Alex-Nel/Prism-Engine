#include "../include/AssetManager.hpp"


extern "C"
{
    #include "../../assets/asset_manager.h"
    #include "../../audio/audio.h"
}


namespace Prism
{
    // ==========================================
    // Internal Casting Helpers
    // ==========================================

    // static inline ::MeshHandle ToCore(const Prism::MeshHandle& h) { return { h.id }; }
    // static inline Prism::MeshHandle FromCore(const ::MeshHandle& h) { return { h.id }; }

    // static inline ::TextureHandle ToCore(const Prism::TextureHandle& h) { return { h.id }; }
    // static inline Prism::TextureHandle FromCore(const ::TextureHandle& h) { return { h.id }; }

    // static inline ::ShaderHandle ToCore(const Prism::ShaderHandle& h) { return { h.id }; }
    // static inline Prism::ShaderHandle FromCore(const ::ShaderHandle& h) { return { h.id }; }

    // static inline ::MaterialHandle ToCore(const Prism::MaterialHandle& h) { return { h.id }; }
    // static inline Prism::MaterialHandle FromCore(const ::MaterialHandle& h) { return { h.id }; }



    // ==========================================
    // LOADING METHODS
    // ==========================================
    
    Model AssetManager::LoadModel(const std::string& name, const std::string& filepath){
        ::Model* raw_model = ::Asset_LoadModel(name.c_str(), filepath.c_str());
        return Prism::Model(raw_model);
    }

    Mesh AssetManager::LoadMesh(const std::string& name, const std::string& filepath) {
        ::Mesh* h = ::Asset_LoadMesh(name.c_str(), filepath.c_str());
        return Prism::Mesh(h);
    }

    Shader AssetManager::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
        ::Shader* h = ::Asset_LoadShader(name.c_str(), vertPath.c_str(), fragPath.c_str());
        return Prism::Shader(h);
    }
    
    Texture AssetManager::LoadTexture(const std::string& name, const std::string& filepath) {
        ::Texture* h = ::Asset_LoadTexture(name.c_str(), filepath.c_str());
        return Prism::Texture(h);
    }

    AudioClip AssetManager::LoadAudio(const std::string& filepath) {
        ::AudioClipHandle raw_handle = ::Audio_LoadClip(filepath.c_str());
        return Prism::AudioClip(raw_handle.id);
    }



    // ==========================================
    // MATERIAL MANAGEMENT
    // ==========================================
    
    Material AssetManager::CreateMaterial(Shader shader, Texture diffuse) {
        ::Material* h = ::Asset_CreateMaterial((::Shader*)shader.GetRaw(), (::Texture*)diffuse.GetRaw());
        return Prism::Material(h);
    }



    // ==========================================
    // BUILT-IN ASSETS
    // ==========================================
    
    Mesh AssetManager::GetBuiltinQuad() {
        ::Mesh* h = ::Asset_GetBuiltinQuad();
        return Prism::Mesh(h);
    }
    
    Mesh AssetManager::GetBuiltinCube() {
        ::Mesh* h = ::Asset_GetBuiltinCube();
        return Prism::Mesh(h);
    }
    
    Mesh AssetManager::GetBuiltinSphere() {
        ::Mesh* h = ::Asset_GetBuiltinSphere();
        return Prism::Mesh(h);
    }
    
    Texture AssetManager::GetDefaultTexture() {
        return Prism::Texture(::Asset_GetDefaultTexture());
    }

    Shader AssetManager::GetDefaultShader() {
        return Prism::Shader(::Asset_GetDefaultShader());
    }
}