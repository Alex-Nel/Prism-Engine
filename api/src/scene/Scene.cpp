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

    void Scene::SetMainCamera(Entity cameraEntity) {
        // Reconstruct the C entity to pass to the backend
        ::Entity raw_cam = { cameraEntity.id, static_cast<::Scene*>(cameraEntity.scene_ptr) };
        ::Scene_SetMainCamera(static_cast<::Scene*>(m_RawScene), raw_cam);
    }



    // --- Physics & Raycasting ---
    
    bool Scene::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, int collisionMask) {
        ::RaycastHit raw_hit;
        bool hit = ::Scene_Raycast(static_cast<::Scene*>(m_RawScene), 
                                   {origin.x, origin.y, origin.z}, 
                                   {direction.x, direction.y, direction.z}, 
                                   maxDistance, &raw_hit, collisionMask);
        
        if (hit)
        {
            outHit.point = Vector3(raw_hit.point.x, raw_hit.point.y, raw_hit.point.z);
            outHit.normal = Vector3(raw_hit.normal.x, raw_hit.normal.y, raw_hit.normal.z);
            outHit.distance = raw_hit.distance;
            outHit.entity = Entity(raw_hit.entity_id, m_RawScene); // Map the C entity back to a C++ Entity
        }

        return hit;
    }

    int Scene::RaycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit* outHits, int maxHits, int collisionMask) {
        // Create a temporary array for the backend
        ::RaycastHit* raw_hits = new ::RaycastHit[maxHits];
        
        int hit_count = ::Scene_RaycastAll(static_cast<::Scene*>(m_RawScene), 
                                           {origin.x, origin.y, origin.z}, 
                                           {direction.x, direction.y, direction.z}, 
                                           maxDistance, raw_hits, maxHits, collisionMask);
        
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