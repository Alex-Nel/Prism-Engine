#include "../../include/scene/Scene.hpp"


extern "C"
{
    #include "../../../scene/scene.h"
}


namespace Prism
{
    // A pointer to the current active scene
    static ::Scene* GlobalActiveScene = nullptr;


    // --- Lifecycle ---

    Scene Scene::Create() {
        ::Scene* newScene = ::Scene_Create();
        GlobalActiveScene = newScene;
        return Scene(newScene);
    }

    void Scene::Destroy() {
        if (m_RawScene) {
            ::Scene_Destroy(static_cast<::Scene*>(m_RawScene));
            m_RawScene = nullptr;
            GlobalActiveScene = nullptr;
        }
    }

    Scene Scene::GetActive() {
        return Scene(GlobalActiveScene);
    }



    // --- Serialization ---
    
    bool Scene::Save(const std::string& filepath) {
        return ::Scene_Save(static_cast<::Scene*>(m_RawScene), filepath.c_str());
    }

    bool Scene::Load(const std::string& filepath) {
        return ::Scene_Load(static_cast<::Scene*>(m_RawScene), filepath.c_str());
    }



    // --- Entity Management ---
    
    Entity Scene::CreateEntity(const std::string& name) {
        ::Entity raw_e = ::Entity_Create(static_cast<::Scene*>(m_RawScene), name.c_str());
        return Entity(raw_e.id, raw_e.scene);
    }

    Entity Scene::GetEntityByName(const std::string& name) {
        ::Entity raw_e = ::Scene_GetEntity(static_cast<::Scene*>(m_RawScene), name.c_str());
        return Entity(raw_e.id, raw_e.scene);
    }

    uint32_t Scene::GetTotalEntityCount() {
        return ::Scene_GetTotalEntityCount(static_cast<::Scene*>(this->m_RawScene));
    }

    uint32_t Scene::GetActiveEntityCount() {
        return ::Scene_GetActiveEntityCount(static_cast<::Scene*>(this->m_RawScene));
    }

    std::vector<Prism::Entity> Scene::GetEntitiesWithTag(const std::string& tag) {
        std::vector<Prism::Entity> results;
        ::Entity c_results[MAX_ENTITIES];
        uint32_t found_count = ::Scene_GetEntitiesWithTag(static_cast<::Scene*>(this->m_RawScene), tag.c_str(), c_results, MAX_ENTITIES);

        for (uint32_t i = 0; i < found_count; i++)
            results.push_back(Prism::Entity(c_results[i].id, c_results[i].scene));

        return results;
    }

    void Scene::SetMainCamera(Entity cameraEntity) {
        // Reconstruct the C entity to pass to the backend
        ::Entity raw_cam = { cameraEntity.id, static_cast<::Scene*>(cameraEntity.scene_ptr) };
        ::Scene_SetMainCamera(static_cast<::Scene*>(m_RawScene), raw_cam);
    }

    void Scene::SetSkybox(Prism::Texture* skybox_text, Prism::Shader* custom_shader) {
        ::Shader* c_shader;

        if (custom_shader != nullptr)
            c_shader = static_cast<::Shader*>(custom_shader->GetRaw());

        ::Texture* c_tex = static_cast<::Texture*>(skybox_text->GetRaw());

        if (!c_tex || !c_shader)
            Debug_Error("Failed to set skybox");

        ::Scene_SetSkybox(static_cast<::Scene*>(m_RawScene), c_tex, c_shader);
    }

    void Scene::SetSkybox(Prism::Color color) {
        ::Scene* c_scene = static_cast<::Scene*>(m_RawScene);

        c_scene->skybox.background_color = ::Color{color.r, color.g, color.b, color.a};
        c_scene->has_skybox = false;
    }

    void Scene::RemoveSkybox() {
        ::Scene_RemoveSkybox(static_cast<::Scene*>(m_RawScene));
    }



    // --- Physics & Raycasting ---
    
    bool Scene::Raycast(const Ray ray, float maxDistance, RaycastHit& outHit, bool hit_triggers, int collisionMask) {
        ::RaycastHit raw_hit;
        ::Ray c_ray = {
            .origin = {ray.origin.x, ray.origin.y, ray.origin.z},
            .direction = {ray.direction.x, ray.direction.y, ray.direction.z}
        };

        bool hit = ::Scene_Raycast(static_cast<::Scene*>(m_RawScene), c_ray, maxDistance, &raw_hit, collisionMask, hit_triggers);
        
        if (hit)
        {
            outHit.point = Vector3(raw_hit.point.x, raw_hit.point.y, raw_hit.point.z);
            outHit.normal = Vector3(raw_hit.normal.x, raw_hit.normal.y, raw_hit.normal.z);
            outHit.distance = raw_hit.distance;
            outHit.entity = Entity(raw_hit.entity_id, m_RawScene); // Map the C entity back to a C++ Entity
        }

        return hit;
    }

    int Scene::RaycastAll(const Ray ray, float maxDistance, RaycastHit* outHits, int maxHits, bool hit_triggers, int collisionMask) {
        // Create a temporary array for the backend
        ::RaycastHit* raw_hits = new ::RaycastHit[maxHits];
        ::Ray c_ray = {
            .origin = {ray.origin.x, ray.origin.y, ray.origin.z},
            .direction = {ray.direction.x, ray.direction.y, ray.direction.z}
        };
        
        int hit_count = ::Scene_RaycastAll(static_cast<::Scene*>(m_RawScene), c_ray, maxDistance, raw_hits, maxHits, collisionMask, hit_triggers);
        
        // Map the results back to the user's array
        for (int i = 0; i < hit_count; i++)
        {
            outHits[i].point = Vector3(raw_hits[i].point.x, raw_hits[i].point.y, raw_hits[i].point.z);
            outHits[i].normal = Vector3(raw_hits[i].normal.x, raw_hits[i].normal.y, raw_hits[i].normal.z);
            outHits[i].distance = raw_hits[i].distance;
            outHits[i].entity = Entity(raw_hits[i].entity_id, m_RawScene);
        }
        
        delete[] raw_hits;
        return hit_count;
    }
}