#pragma once

#include "Components.hpp"
#include <string>

namespace Prism
{
    // Global pointer to the current active scene
    inline ::Scene* GlobalActiveScene = nullptr;



    class Entity
    {
    public:
        ::Entity raw; 

        Entity() : raw({ENTITY_NONE, nullptr}) {}
        Entity(::Entity e) : raw(e) {}
        
        bool IsValid() const {
            return ::Entity_IsValid(raw);
        }
        void Destroy() {
            ::Entity_Destroy(raw);
        }
        void SetActive(bool active) {
            ::Entity_SetActive(raw, active);
        }



        // --- Hierarchy Functions ---

        void SetParent(Entity parent) {
            ::Entity_SetParent(raw, parent.raw);
        }
        void RemoveParent() {
            ::Entity_RemoveParent(raw);
        }



        // --- Component Setters ---
        
        void SetName(const std::string& name) { 
            ::Entity_SetName(raw, name.c_str()); 
        }

        void AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale) {
            ::Entity_AddTransform(raw, pos, rot, scale);
        }

        void AddRenderable(::MeshHandle mesh, ::MaterialHandle material) {
            ::Entity_AddRenderable(raw, mesh.id, material.id);
        }

        void AddCamera(float fovDegrees) {
            float fov_radians = fovDegrees * (3.14159265f / 180.0f);
            ::Entity_AddCamera(raw, fov_radians, 0.1f, 1000.0f);
        }

        void AddPointLight(const Prism::Vector3& color) {
            ::Entity_AddPointLight(raw, color, 1.0f, 1.0f, 0.09f, 0.032f);
        }

        void AddRigidbody(float mass) {
            ::Entity_AddRigidbody(raw, mass);
        }

        void AddColliderBox(const Prism::Vector3& extents, bool is_trigger = false) {
            ::Entity_AddColliderBox(raw, extents, is_trigger);
        }

        void AddColliderBoxAuto(bool is_trigger = false) {
            ::Entity_AddColliderBoxAuto(raw, is_trigger);
        }

        void AddColliderSphere(float radius, bool is_trigger = false) {
            ::Entity_AddColliderSphere(raw, radius, is_trigger);
        }

        void AddColliderMesh(::MeshHandle mesh, bool is_trigger = false) {
            // Note: Mesh colliders should usually be static (mass = 0)
            ::Entity_AddColliderMesh(raw, mesh, is_trigger);
        }



        // --- Component Getters ---
        
        Prism::Transform* GetTransform() {
            return reinterpret_cast<Prism::Transform*>(::Entity_GetTransform(raw));
        }

        Prism::RigidbodyComponent* GetRigidbody() {
            return reinterpret_cast<Prism::RigidbodyComponent*>(::Entity_GetRigidbody(raw));
        }

        Prism::ColliderComponent* GetCollider() {
            return reinterpret_cast<Prism::ColliderComponent*>(::Entity_GetCollider(raw));
        }

        ::CameraComponent* GetCamera() {
            return ::Entity_GetCamera(raw);
        }



        // --- Utility functions ---

        static Entity Find(const std::string& name)
        {
            if (!GlobalActiveScene) {
                Debug_Warn("No active scene");
                return Entity();
            }

            return Entity(::Scene_GetEntity(GlobalActiveScene, name.c_str()));
        }



        // --- Custom script getters/setters (implemented in Behavior.hpp) ---

        // Adds a custom script of type T to an entity and returns a pointer to the new script instance.
        template<typename T>
        T* AddScript();

        // Returns the custom script of type T, or nullptr if not found.
        template<typename T>
        T* GetScript();
    };
}