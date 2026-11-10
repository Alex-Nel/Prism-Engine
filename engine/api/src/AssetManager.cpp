#include "../include/AssetManager.hpp"


extern "C"
{
    #include <string.h>
    #include "../../assets/asset_manager.h"
    #include "../../audio/audio.h"
}


namespace Prism
{
    // Asset manager used by the engine instance
    static ::AssetManager* s_asset_manager = nullptr;



    // ==========================================
    // Model implementation
    // ==========================================

    void* Model::GetRawSkeleton() const {
        ::Model* c_Model = static_cast<::Model*>(m_Handle);
        return c_Model->skeleton;
    }

    uint32_t Model::GetAnimationCount() const {
        if (!IsValid())
            return 0;
        return static_cast<::Model*>(m_Handle)->animation_count;
    }

    Prism::AnimationClip Model::GetAnimation(uint32_t index) const {
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        if (index >= c_model->animation_count)
            return Prism::AnimationClip(nullptr);
        return Prism::AnimationClip(c_model->animations[index]);
    }

    Prism::AnimationClip Model::GetAnimation(const std::string& name) const {
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        
        // Search the array for a matching name
        for (uint32_t i = 0; i < c_model->animation_count; i++) {
            if (strcmp(c_model->animations[i]->name, name.c_str()) == 0) {
                return Prism::AnimationClip(c_model->animations[i]);
            }
        }
        return Prism::AnimationClip(nullptr); // Not found
    }

    Prism::Material Model::GetMaterial(uint32_t mesh_index) const {
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        if (mesh_index >= c_model->node_count)
            return Prism::Material(nullptr);
        return Prism::Material(c_model->nodes[mesh_index].material);
    }

    uint32_t Model::GetMeshCount() const {
        if (!IsValid()) return 0;
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        return c_model->node_count;
    }

    bool Model::IsMeshSkinned(uint32_t index) const {
        if (!IsValid()) return 0;
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        return c_model->nodes[index].is_skinned;
    }

    Prism::Mesh Model::GetMesh(uint32_t index) const {
        if (!IsValid()) return Prism::Mesh(nullptr);
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        if (index >= c_model->node_count) {
            return Prism::Mesh(nullptr);
        }
        return Prism::Mesh(c_model->nodes[index].mesh);
    }

    Prism::SkinnedMesh Model::GetSkinnedMesh(uint32_t index) const {
        if (!IsValid()) return Prism::SkinnedMesh(nullptr);
        ::Model* c_model = static_cast<::Model*>(m_Handle);
        if (index >= c_model->node_count) {
            return Prism::SkinnedMesh(nullptr);
        }
        return Prism::SkinnedMesh(c_model->nodes[index].mesh);
    }



    // ==========================================
    // Asset Manager loading methods
    // ==========================================
    
    Model AssetManager::LoadModel(const std::string& name, const std::string& filepath){
        ::Model* raw_model = ::Asset_LoadModel(s_asset_manager, name.c_str(), filepath.c_str());
        return Prism::Model(raw_model);
    }

    Shader AssetManager::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
        ::Shader* h = ::Asset_LoadShader(s_asset_manager, name.c_str(), vertPath.c_str(), fragPath.c_str());
        return Prism::Shader(h);
    }
    
    AudioClip AssetManager::LoadAudio(const std::string& filepath) {
        ::AudioClipHandle raw_handle = ::Audio_LoadClip(filepath.c_str());
        return Prism::AudioClip(raw_handle.id);
    }



    Model AssetManager::GetModelByName(const std::string& name) {
        ::Model* c_mod = ::Asset_GetModelByName(s_asset_manager, name.c_str());
        return Prism::Model(c_mod);
    }

    Mesh AssetManager::GetMeshByName(const std::string& name) {
        ::Mesh* c_mesh = ::Asset_GetMeshByName(s_asset_manager, name.c_str());
        return Prism::Mesh(c_mesh);
    }

    SkinnedMesh AssetManager::GetSkinnedMeshByName(const std::string& name) {
        ::SkinnedMesh* c_mesh = ::Asset_GetSkinnedMeshByName(s_asset_manager, name.c_str());
        return Prism::SkinnedMesh(c_mesh);
    }

    Texture AssetManager::GetTextureByName(std::string name) {
        ::Texture* c_text = ::Asset_GetTextureByName(s_asset_manager, name.c_str());
        return Prism::Texture(c_text);
    }

    Material AssetManager::GetMaterialByName(std::string name) {
        ::Material* c_mat = ::Asset_GetMaterialByName(s_asset_manager, name.c_str());
        return Prism::Material(c_mat);
    }

    Font AssetManager::GetFontByName(const std::string& name) {
        return Prism::Font(::Asset_GetFontByName(s_asset_manager, name.c_str()));
    }



    void AssetManager::UnloadModel(Model model) {
        ::Asset_UnloadModel(s_asset_manager, (::Model*)model.GetRawModel());
    }

    void AssetManager::UnloadMesh(Mesh mesh) {
        ::Asset_UnloadMesh(s_asset_manager, (::Mesh*)mesh.GetRaw());
    }

    void AssetManager::UnloadSkinnedMesh(SkinnedMesh mesh) {
        ::Asset_UnloadSkinnedMesh(s_asset_manager, (::SkinnedMesh*)mesh.GetRaw());
    }

    void AssetManager::UnloadTexture(Texture texture) {
        ::Asset_UnloadTexture(s_asset_manager, (::Texture*)texture.GetRaw());
    }

    void AssetManager::UnloadFont(Font font) {
        ::Asset_UnloadFont(s_asset_manager, (::Font*)font.GetRaw());
    }

    void AssetManager::UnloadShader(Shader shader) {
        ::Asset_UnloadShader(s_asset_manager, (::Shader*)shader.GetRaw());
    }

    void AssetManager::UnloadEnvironmentMap(EnvironmentMap env_map) {
        ::Asset_UnloadEnvironmentMap(s_asset_manager, (::EnvironmentMap*)env_map.GetRaw());
    }

    void AssetManager::UnloadMaterial(Material material) {
        ::Asset_UnloadMaterial(s_asset_manager, (::Material*)material.GetRaw());
    }

    void AssetManager::UnloadAudioClip(AudioClip clip) {
        ::AudioClipHandle raw_handle = { clip.GetID() };
        ::Audio_DestroyClip(raw_handle);
    }



    // ==========================================
    // Texture Management
    // ==========================================

    Texture AssetManager::LoadTexture(const std::string& name, const std::string& filepath) {
        ::Texture* h = ::Asset_LoadTexture(s_asset_manager, name.c_str(), filepath.c_str());
        return Prism::Texture(h);
    }

    Texture AssetManager::LoadCubemapTexture(const std::string& name, const std::string& right, const std::string& left, const std::string& top, const std::string& bottom, const std::string& front, const std::string& back) {
        ::Texture* t = ::Asset_LoadCubemapTexture(s_asset_manager, name.c_str(), right.c_str(), left.c_str(), top.c_str(), bottom.c_str(), front.c_str(), back.c_str());
        return Prism::Texture(t);
    }

    EnvironmentMap AssetManager::LoadEnvironmentMap(const std::string& filepath) {
        ::EnvironmentMap* c_env = ::Asset_LoadEnvironmentMap(s_asset_manager, filepath.c_str());
        return Prism::EnvironmentMap(c_env);
    }

    EnvironmentMap AssetManager::LoadEnvironmentMapFromSkybox(const std::string& name, const std::string& right, const std::string& left, const std::string& top, const std::string& bottom, const std::string& front, const std::string& back) {
        ::EnvironmentMap* c_env = ::Asset_LoadEnvironmentMapFromSkybox(s_asset_manager, name.c_str(), right.c_str(), left.c_str(), top.c_str(), bottom.c_str(), front.c_str(), back.c_str());
        return Prism::EnvironmentMap(c_env);
    }

    Texture AssetManager::CreateSolidColorTexture(const std::string& name, Prism::Color color) {
        ::Color c_color = { color.r, color.g, color.b, color.a };
        ::Texture* raw_tex = ::Asset_CreateSolidColorTexture(s_asset_manager, name.c_str(), c_color);
        return Prism::Texture(raw_tex);
    }

    Font AssetManager::LoadFont(const std::string& name, const std::string& filepath, float pixel_height) {
        ::Font* h = ::Asset_LoadFont(s_asset_manager, name.c_str(), filepath.c_str(), pixel_height);
        return Prism::Font(h);
    }



    // ==========================================
    // Dynamic Meshes
    // ==========================================

    Mesh AssetManager::CreateDynamicMesh(uint32_t max_vertices, uint32_t max_indices) {
        ::Mesh* raw_mesh = ::Asset_CreateDynamicMesh(s_asset_manager, max_vertices, max_indices);
        return Prism::Mesh(raw_mesh);
    }

    void AssetManager::UpdateDynamicMesh(Mesh mesh, Prism::Vertex3D* vertices, uint32_t vertex_count, uint32_t* indices, uint32_t index_count) {
        ::Asset_UpdateDynamicMesh(s_asset_manager, (::Mesh*)mesh.GetRaw(), (::Vertex3D*)vertices, vertex_count, indices, index_count);
    }



    // ==========================================
    // Material Management
    // ==========================================
    
    Material AssetManager::CreateMaterial(const std::string& name, Texture diffuse) {
        ::Material* h = ::Asset_CreateMaterial(s_asset_manager, name.c_str(), (::Texture*)diffuse.GetRaw(), NULL);
        return Prism::Material(h);
    }



    // ==========================================
    // Built-In Assets
    // ==========================================
    
    Mesh AssetManager::GetBuiltinQuad() {
        ::Mesh* h = ::Asset_GetBuiltinQuad(s_asset_manager);
        return Prism::Mesh(h);
    }
    
    Mesh AssetManager::GetBuiltinCube() {
        ::Mesh* h = ::Asset_GetBuiltinCube(s_asset_manager);
        return Prism::Mesh(h);
    }
    
    Mesh AssetManager::GetBuiltinSphere() {
        ::Mesh* h = ::Asset_GetBuiltinSphere(s_asset_manager);
        return Prism::Mesh(h);
    }
    
    Texture AssetManager::GetDefaultTexture() {
        return Prism::Texture(::Asset_GetDefaultTexture(s_asset_manager));
    }





    void* AssetManager::GetGlobalInstance() {
        return s_asset_manager;
    }

    void AssetManager::SetGlobalInstance(void* am_instance) {
        s_asset_manager = static_cast<::AssetManager*>(am_instance);
    }
}