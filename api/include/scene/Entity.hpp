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

    struct Ray;
    struct RaycastHit;

    struct Transform;
    struct RenderComponent;
    struct RigidbodyComponent;
    struct ColliderComponent;
    struct CameraComponent;
    struct LightComponent;
    struct AudioListenerComponent;
    struct AudioSourceComponent;
    struct AnimatorComponent;
    struct BoneAttachmentComponent;
    struct LineRendererComponent;
    struct SpriteRendererComponent;



    // Enum for types of lights
    enum LightType
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };



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
        void SetParent(Entity parent, const char* bone_name, Prism::Matrix4 local_offset = Prism::Matrix4::Identity());
        Prism::Entity GetParent();
        void RemoveParent();
        void SetTag(const std::string& tag);
        std::string GetTag() const;
        bool CompareTag(const std::string& tag) const;
        void AddModel(Prism::Model model);
        std::vector<Prism::Entity> GetChildren(bool recursive = true);
        Prism::Entity FindChildByName(const std::string& name) const;
        Prism::Ray ScreenPointToRay(const Prism::Vector2& mouse_pos); // TODO: Make this a component method instead



        // --- Component Setters ---
        
        std::string SetName(const std::string& name);
        Prism::Transform* AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
        Prism::RenderComponent* AddRenderable(Prism::Mesh mesh, Prism::Material material);
        Prism::CameraComponent* AddCamera(float fovDegrees);
        Prism::LightComponent* AddLight(Prism::LightType type, const Prism::Color& color);
        Prism::RigidbodyComponent* AddRigidbody(float mass);
        Prism::ColliderComponent* AddColliderBox(const Prism::Vector3 extents = Prism::Vector3{1, 1, 1}, bool is_trigger = false);
        Prism::ColliderComponent* AddColliderBoxAuto(bool is_trigger = false);
        Prism::ColliderComponent* AddColliderSphere(float radius = 0.5f, bool is_trigger = false);
        Prism::ColliderComponent* AddColliderMesh(Prism::Mesh mesh, bool is_trigger = false, bool is_convex = false);
        Prism::AudioListenerComponent* AddAudioListener();
        Prism::AudioSourceComponent* AddAudioSource();
        Prism::AnimatorComponent* AddAnimator(void* raw_skeleton, const Prism::AnimationClip& default_clip);
        Prism::LineRendererComponent* AddLineRenderer(Prism::Material* mat = nullptr);
        Prism::SpriteRendererComponent* AddSpriteRenderer(Prism::Material* mat = nullptr);



        // --- Component Getters ---
        
        std::string GetName();
        Prism::Transform* GetTransform();
        Prism::RenderComponent* GetRenderable();
        Prism::RigidbodyComponent* GetRigidbody();
        Prism::ColliderComponent* GetCollider();
        Prism::CameraComponent* GetCamera();
        Prism::LightComponent* GetLight();
        Prism::AudioListenerComponent* GetAudioListener();
        Prism::AudioSourceComponent* GetAudioSource();
        Prism::AnimatorComponent* GetAnimator();
        Prism::BoneAttachmentComponent* GetBoneAttachment();
        Prism::LineRendererComponent* GetLineRenderer();
        Prism::SpriteRendererComponent* GetSpriteRenderer();



        // --- Component Hierarchy Getters ---

        std::vector<Prism::Transform*> GetTransformsInChildren(bool recursive = true);
        std::vector<Prism::RenderComponent*> GetRenderablesInChildren(bool recursive = true);
        std::vector<Prism::RigidbodyComponent*> GetRigidbodiesInChildren(bool recursive = true);
        std::vector<Prism::ColliderComponent*> GetCollidersInChildren(bool recursive = true);
        std::vector<Prism::CameraComponent*> GetCamerasInChildren(bool recursive = true);
        std::vector<Prism::LightComponent*> GetLightsInChildren(bool recursive = true);
        std::vector<Prism::AudioListenerComponent*> GetAudioListenersInChildren(bool recursive = true);
        std::vector<Prism::AudioSourceComponent*> GetAudioSourcesInChildren(bool recursive = true);
        std::vector<Prism::AnimatorComponent*> GetAnimatorsInChildren(bool recursive = true);
        std::vector<Prism::BoneAttachmentComponent*> GetBoneAttachmentsInChildren(bool recursive = true);
        std::vector<Prism::LineRendererComponent*> GetLineRenderersInChildren(bool recursive = true);
        std::vector<Prism::SpriteRendererComponent*> GetSpriteRenderersInChildren(bool recursive = true);
        
        
        Prism::Transform* GetTransformInParent();
        Prism::RenderComponent* GetRenderableInParent();
        Prism::RigidbodyComponent* GetRigidbodyInParent();
        Prism::ColliderComponent* GetColliderInParent();
        Prism::CameraComponent* GetCameraInParent();
        Prism::LightComponent* GetLightInParent();
        Prism::AudioListenerComponent* GetAudioListenerInParent();
        Prism::AudioSourceComponent* GetAudioSourceInParent();
        Prism::AnimatorComponent* GetAnimatorInParent();
        Prism::BoneAttachmentComponent* GetBoneAttachmentInParent();
        Prism::LineRendererComponent* GetLineRendererInParent();
        Prism::SpriteRendererComponent* GetSpriteRendererInParent();



        // --- Component Removers ---

        void RemoveRenderable();
        void RemoveRigidbody();
        void RemoveCollider();
        void RemoveCamera();
        void RemoveLight();
        void RemoveAudioListener();
        void RemoveAudioSource();
        void RemoveAnimator();
        void RemoveLineRenderer();
        void RemoveSpriteRenderer();



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