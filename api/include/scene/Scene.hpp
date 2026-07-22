#pragma once

#include "Entity.hpp"
#include "Components.hpp"
#include "../PrismAPI.hpp"

namespace Prism
{

    // Structure for a scene ray
    typedef struct Ray
    {
        Prism::Vector3 origin;
        Prism::Vector3 direction;
    } Ray;



    // Structure for raycast information
    struct RaycastHit
    {
        bool hit;
        Prism::Entity entity;
        Prism::Vector3 point;
        Prism::Vector3 normal;
        float distance;
    };



    // --- Class containing all scene related functions ---
    class PRISM_API Scene
    {
    private:
        void* m_RawScene;

    public:
        Scene(void* scene) : m_RawScene(scene) {}
        void* GetRaw() const { return m_RawScene; }



        // --- LIFECYCLE ---

        static Scene Create();      // Allocates and initializes a new scene on the heap
        void Destroy();             // Destroys the scene and frees the memory
        static Scene GetActive();   // Returns a wrapper to the currently active scene



        // --- Serialization ---
        
        bool Save(const std::string& filepath);
        bool Load(const std::string& filepath);



        // --- Entity Management ---
        
        Entity CreateEntity(const std::string& name);
        Entity GetEntityByName(const std::string& name);
        uint32_t GetTotalEntityCount();
        uint32_t GetActiveEntityCount();
        std::vector<Prism::Entity> GetEntitiesWithTag(const std::string& tag);
        void SetMainCamera(Entity cameraEntity);
        void SetGravity(Prism::Vector3 gravity);
        Vector3 GetGravity();
        void SetSkybox(Prism::Texture* skybox_text, Prism::Shader* custom_shader = nullptr);
        void SetSkybox(Prism::Color color);
        void RemoveSkybox();
        void SetGlobalAmbientColor(Prism::Color color);
        void SetGlobalAmbientIllumination(float illumination);
        Prism::Color GetGlobalAmbientColor();
        float GetGlobalAmbientIllumination();


        
        // --- Physics & Raycasting ---
        
        bool Raycast(const Prism::Ray ray, float maxDistance, RaycastHit& outHit, bool hit_triggers = false, int collisionMask = COLLISION_MASK_ALL);
        int RaycastAll(const Prism::Ray ray, float maxDistance, RaycastHit* outHits, int maxHits, bool hit_triggers = false, int collisionMask = COLLISION_MASK_ALL);
    };
}