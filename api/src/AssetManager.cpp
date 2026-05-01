#include "../include/AssetManager.hpp"


extern "C"
{
    #include "../../assets/asset_manager.h"
}


namespace Prism
{
    // ==========================================
    // Internal Casting Helpers
    // ==========================================

    static inline ::MeshHandle ToCore(const Prism::MeshHandle& h) { return { h.id }; }
    static inline Prism::MeshHandle FromCore(const ::MeshHandle& h) { return { h.id }; }

    static inline ::TextureHandle ToCore(const Prism::TextureHandle& h) { return { h.id }; }
    static inline Prism::TextureHandle FromCore(const ::TextureHandle& h) { return { h.id }; }

    static inline ::ShaderHandle ToCore(const Prism::ShaderHandle& h) { return { h.id }; }
    static inline Prism::ShaderHandle FromCore(const ::ShaderHandle& h) { return { h.id }; }

    static inline ::MaterialHandle ToCore(const Prism::MaterialHandle& h) { return { h.id }; }
    static inline Prism::MaterialHandle FromCore(const ::MaterialHandle& h) { return { h.id }; }



    // ==========================================
    // LOADING METHODS
    // ==========================================
    
    MeshRef AssetManager::LoadMesh(const std::string& name, const std::string& filepath) {
        ::MeshHandle h = ::Asset_LoadMesh(name.c_str(), filepath.c_str());
        return { FromCore(h), GetMeshData(FromCore(h)) };
    }

    ShaderHandle AssetManager::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
        ::ShaderHandle h = ::Asset_LoadShader(name.c_str(), vertPath.c_str(), fragPath.c_str());
        return FromCore(h);
    }
    
    TextureHandle AssetManager::LoadTexture(const std::string& name, const std::string& filepath) {
        ::TextureHandle h = ::Asset_LoadTexture(name.c_str(), filepath.c_str());
        return FromCore(h);
    }



    // ==========================================
    // MATERIAL MANAGEMENT
    // ==========================================
    
    MaterialRef AssetManager::CreateMaterial(ShaderHandle shader, TextureHandle diffuse) {
        ::MaterialHandle h = ::Asset_CreateMaterial(ToCore(shader), ToCore(diffuse));
        return { FromCore(h), GetMaterial(FromCore(h)) };
    }
    
    Material* AssetManager::GetMaterial(MaterialHandle handle) {
        // Since the Material API perfectly mirrors the backend ::Material struct, we can reinterpret_cast the pointer
        return reinterpret_cast<Prism::Material*>(::Asset_GetMaterial(ToCore(handle)));
    }



    // ==========================================
    // BUILT-IN ASSETS
    // ==========================================
    
    MeshRef AssetManager::GetBuiltinQuad() {
        ::MeshHandle h = ::Asset_GetBuiltinQuad();
        return { FromCore(h), GetMeshData(FromCore(h)) };
    }
    
    MeshRef AssetManager::GetBuiltinCube() {
        ::MeshHandle h = ::Asset_GetBuiltinCube();
        return { FromCore(h), GetMeshData(FromCore(h)) };
    }
    
    MeshRef AssetManager::GetBuiltinSphere() {
        ::MeshHandle h = ::Asset_GetBuiltinSphere();
        return { FromCore(h), GetMeshData(FromCore(h)) };
    }
    
    TextureHandle AssetManager::GetDefaultTexture() {
        return FromCore(::Asset_GetDefaultTexture());
    }

    ShaderHandle AssetManager::GetDefaultShader() {
        return FromCore(::Asset_GetDefaultShader());
    }


    
    // ==========================================
    // UTILITY
    // ==========================================
    
    Prism::MeshData* AssetManager::GetMeshData(MeshHandle handle) {
        // Safe pointer cast because Prism::MeshData maps exactly to ::MeshData
        return reinterpret_cast<Prism::MeshData*>(::Asset_GetMeshData(ToCore(handle)));
    }
}