#pragma once

#include "Entity.hpp"
#include "Components.hpp"

namespace Prism
{

    // Structure for raycast information
    struct RaycastHit
    {
        Prism::Vector3 point;
        Prism::Vector3 normal;
        float distance;
        Prism::Entity entity;
    };



    // --- Class containing all scene related functions ---
    class Scene
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
        void SetMainCamera(Entity cameraEntity);


        
        // --- Physics & Raycasting ---
        
        bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit, int collisionMask = COLLISION_MASK_ALL);
        int RaycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit* outHits, int maxHits, int collisionMask = COLLISION_MASK_ALL);
    };
}