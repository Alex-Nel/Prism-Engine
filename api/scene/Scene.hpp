#pragma once

#include "Entity.hpp"

namespace Prism
{

    class Scene
    {
    private:
        ::Scene* m_RawScene;

    public:
        // Bind the wrapper to the active scene pointer
        Scene(::Scene* scene) : m_RawScene(scene) {}

        ::Scene* GetRaw() const { return m_RawScene; }



        // --- LIFECYCLE ---

        // Allocates and initializes a new scene on the heap
        static Scene Create() {
            ::Scene* newScene = ::Scene_Create();
            GlobalActiveScene = newScene;
            return Scene(newScene);
        }

        // Destroys the scene and frees the memory
        void Destroy() {
            if (m_RawScene) {
                ::Scene_Destroy(m_RawScene);
                m_RawScene = nullptr;
                GlobalActiveScene = nullptr;
            }
        }

        // Returns a wrapper to the currently active scene
        static Scene GetActive() {
            if (!GlobalActiveScene) {
                // Return an invalid scene wrapper safely
                return Scene(nullptr);
            }
            return Scene(GlobalActiveScene);
        }



        // --- Serialization ---
        
        bool Save(const std::string& filepath) {
            return ::Scene_Save(m_RawScene, filepath.c_str());
        }

        bool Load(const std::string& filepath) {
            return ::Scene_Load(m_RawScene, filepath.c_str());
        }



        // --- Entity Management ---
        
        Entity CreateEntity(const std::string& name) {
            return Entity(::Entity_Create(m_RawScene, name.c_str()));
        }

        Entity GetEntityByName(const std::string& name) {
            return Entity(::Scene_GetEntity(m_RawScene, name.c_str()));
        }

        void SetMainCamera(Entity cameraEntity) {
            ::Scene_SetMainCamera(m_RawScene, cameraEntity.raw);
        }


        
        // --- Physics & Raycasting ---
        
        bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, ::RaycastHit& outHit, int collisionMask = COLLISION_MASK_ALL) {
            return ::Scene_Raycast(m_RawScene, origin, direction, maxDistance, &outHit, collisionMask);
        }

        // Returns the number of hits found
        int RaycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, ::RaycastHit* outHits, int maxHits, int collisionMask = COLLISION_MASK_ALL) {
            return ::Scene_RaycastAll(m_RawScene, origin, direction, maxDistance, outHits, maxHits, collisionMask);
        }
    };
}