#pragma once

#include <string>
#include "core/Mesh.hpp"

extern "C"
{
    #include "assets/asset_manager.h"
}

namespace Prism
{

    // ==========================================
    // Asset References
    // ==========================================

    struct MeshRef
    {
        ::MeshHandle handle;
        Prism::MeshData* data;

        // Automatically converts to MeshHandle when needed
        operator ::MeshHandle() const {
            return handle;
        }
    };


    struct MaterialRef
    {
        ::MaterialHandle handle;
        ::Material* data;

        // Automatically converts to a MaterialHandle
        operator ::MaterialHandle() const {
            return handle;
        }
        
        // Allows the user to use -> to access material properties directly
        ::Material* operator->() {
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
        
        static MeshRef LoadMesh(const std::string& name, const std::string& filepath) {
            ::MeshHandle h = ::Asset_LoadMesh(name.c_str(), filepath.c_str());
            return { h, GetMeshData(h) };
        }

        static ::ShaderHandle LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
            return ::Asset_LoadShader(name.c_str(), vertPath.c_str(), fragPath.c_str());
        }
        
        static ::TextureHandle LoadTexture(const std::string& name, const std::string& filepath) {
            return ::Asset_LoadTexture(name.c_str(), filepath.c_str());
        }


        // --- MATERIAL MANAGEMENT ---
        
        static MaterialRef CreateMaterial(::ShaderHandle shader, ::TextureHandle diffuse) {
            ::MaterialHandle h = ::Asset_CreateMaterial(shader, diffuse);
            return { h, GetMaterial(h) };
        }
        
        static ::Material* GetMaterial(::MaterialHandle handle) {
            // No casting needed. Just return pointer.
            return ::Asset_GetMaterial(handle);
        }


        // --- BUILT-IN ASSETS ---
        
        static MeshRef GetBuiltinQuad() {
            ::MeshHandle h = ::Asset_GetBuiltinQuad();
            return { h, GetMeshData(h) };
        }
        
        static MeshRef GetBuiltinCube() {
            ::MeshHandle h = ::Asset_GetBuiltinCube();
            return { h, GetMeshData(h) };
        }
        
        static MeshRef GetBuiltinSphere() {
            ::MeshHandle h = ::Asset_GetBuiltinSphere();
            return { h, GetMeshData(h) };
        }
        
        static ::TextureHandle GetDefaultTexture() {
            return ::Asset_GetDefaultTexture();
        }
        static ::ShaderHandle GetDefaultShader() {
            return ::Asset_GetDefaultShader();
        }

        
        // --- UTILITY ---
        
        static Prism::MeshData* GetMeshData(::MeshHandle handle) {
            return reinterpret_cast<Prism::MeshData*>(::Asset_GetMeshData(handle));
        }
    };
}