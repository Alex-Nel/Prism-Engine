#pragma once

#include <string>
#include <cstdint>
#include "../core/Math.hpp"
#include "../AssetManager.hpp"


namespace Prism
{
    // Forward declerations

    struct Transform;
    struct RigidbodyComponent;
    struct ColliderComponent;
    struct CameraComponent;



    // --- Base Entity Class ---
    class Entity
    {
    public:
        uint32_t id;      // Entities ID in the scene
        void* scene_ptr;  // Pointer to the scene this entity belongs to


        // --- Constructors ---
        
        Entity();
        Entity(uint32_t entity_id, void* scene);


        // --- Entity Lifecycle functions ---

        bool IsValid() const;
        void Destroy();
        void SetActive(bool active);


        // --- Hierarchy Functions ---

        void SetParent(Entity parent);
        void RemoveParent();


        // --- Component Setters ---
        
        void SetName(const std::string& name);
        void AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
        void AddRenderable(MeshHandle mesh, MaterialHandle material);
        void AddCamera(float fovDegrees);
        void AddPointLight(const Prism::Vector3& color);
        void AddRigidbody(float mass);
        void AddColliderBox(const Prism::Vector3& extents, bool is_trigger = false);
        void AddColliderBoxAuto(bool is_trigger = false);
        void AddColliderSphere(float radius, bool is_trigger = false);
        void AddColliderMesh(MeshHandle mesh, bool is_trigger = false);



        // --- Component Getters ---
        
        Prism::Transform* GetTransform();
        Prism::RigidbodyComponent* GetRigidbody();
        Prism::ColliderComponent* GetCollider();
        Prism::CameraComponent* GetCamera();



        // --- Utility functions ---

        static Entity Find(const std::string& name);



        // --- Custom script getters/setters (implemented in Behavior.hpp) ---

        // Adds a custom script of type T to an entity and returns a pointer to the new script instance.
        template<typename T>
        T* AddScript();

        // Returns the custom script of type T, or nullptr if not found.
        template<typename T>
        T* GetScript();
    };
}