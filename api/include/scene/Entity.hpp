#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include "../core/Math.hpp"
#include "../AssetManager.hpp"
#include "../Audio.hpp"
#include "../PrismAPI.hpp"


namespace Prism
{
    // Forward declerations

    struct Transform;
    struct RenderComponent;
    struct RigidbodyComponent;
    struct ColliderComponent;
    struct CameraComponent;
    struct PointLightComponent;
    struct AudioListenerComponent;
    struct AudioSourceComponent;



    // --- Base Entity Class ---
    class PRISM_API Entity
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
        Prism::Entity GetParent();
        void RemoveParent();
        void AddModel(Prism::Model model);
        std::vector<Prism::Entity> GetChildren(bool recursive = true);



        // --- Component Setters ---
        
        void SetName(const std::string& name);
        void AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
        void AddRenderable(Prism::Mesh mesh, Prism::Material material);
        void AddCamera(float fovDegrees);
        void AddPointLight(const Prism::Color& color);
        void AddRigidbody(float mass);
        void AddColliderBox(const Prism::Vector3& extents, bool is_trigger = false);
        void AddColliderBoxAuto(bool is_trigger = false);
        void AddColliderSphere(float radius, bool is_trigger = false);
        void AddColliderMesh(Prism::Mesh mesh, bool is_trigger = false, bool is_convex = false);
        void AddAudioListener();
        void AddAudioSource();



        // --- Component Getters ---
        
        std::string GetName();
        Prism::Transform* GetTransform();
        Prism::RenderComponent* GetRenderable();
        Prism::RigidbodyComponent* GetRigidbody();
        Prism::ColliderComponent* GetCollider();
        Prism::CameraComponent* GetCamera();
        Prism::PointLightComponent* GetPointLight();
        Prism::AudioListenerComponent* GetAudioListener();
        Prism::AudioSourceComponent* GetAudioSource();



        // --- Component Hierarchy Getters ---

        std::vector<Prism::Transform*> GetTransformsInChildren(bool recursive = true);
        std::vector<Prism::RenderComponent*> GetRenderablesInChildren(bool recursive = true);
        std::vector<Prism::RigidbodyComponent*> GetRigidbodiesInChildren(bool recursive = true);
        std::vector<Prism::ColliderComponent*> GetCollidersInChildren(bool recursive = true);
        std::vector<Prism::CameraComponent*> GetCamerasInChildren(bool recursive = true);
        std::vector<Prism::PointLightComponent*> GetPointLightsInChildren(bool recursive = true);
        std::vector<Prism::AudioListenerComponent*> GetAudioListenersInChildren(bool recursive = true);
        std::vector<Prism::AudioSourceComponent*> GetAudioSourcesInChildren(bool recursive = true);
        
        
        Prism::Transform* GetTransformInParent();
        Prism::RenderComponent* GetRenderableInParent();
        Prism::RigidbodyComponent* GetRigidbodyInParent();
        Prism::ColliderComponent* GetColliderInParent();
        Prism::CameraComponent* GetCameraInParent();
        Prism::PointLightComponent* GetPointLightInParent();
        Prism::AudioListenerComponent* GetAudioListenerInParent();
        Prism::AudioSourceComponent* GetAudioSourceInParent();



        // --- Component Removers ---

        void RemoveRenderable();
        void RemoveRigidbody();
        void RemoveCollider();
        void RemoveCamera();
        void RemovePointLight();
        void RemoveAudioListener();
        void RemoveAudioSource();



        // --- Utility functions ---

        static Entity Find(const std::string& name);



        // --- Custom script getters/setters (implemented in Behavior.hpp) ---

        // Adds a custom script of type T to an entity and returns a pointer to the new script instance.
        template<typename T>
        T* AddScript();

        // Returns the custom script of type T, or nullptr if not found.
        template<typename T>
        T* GetScript();

        // Removes the first instance of a script T from an entity
        template<typename T>
        void RemoveScript();

        // Removes a specific instance of a script (if they have multiple of the same type)
        template<typename T>
        void RemoveScript(T* specific_instance);

        // Returns a vector of all script instances in children entities
        template<typename T>
        std::vector<T*> GetScriptsInChildren(bool recursive = true);

        // Returns a script instance in the parent of an entity
        template<typename T>
        T* GetScriptInParent();
    };
}