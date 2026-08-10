#pragma once

#include <string>
#include <cstdint>
#include <utility>
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
    struct MeshRendererComponent;
    struct SkinnedMeshRendererComponent;
    struct RigidbodyComponent;
    struct ColliderComponent;
    struct BoxColliderComponent;
    struct SphereColliderComponent;
    struct MeshColliderComponent;
    struct CameraComponent;
    struct LightComponent;
    struct AudioListenerComponent;
    struct AudioSourceComponent;
    struct AnimatorComponent;
    struct BoneAttachmentComponent;
    struct LineRendererComponent;
    struct SpriteRendererComponent;
    struct ReflectionProbeComponent;



    // Enum for types of lights
    enum LightType
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    };



    namespace Detail
    {
        template<typename...>
        inline constexpr bool AlwaysFalse = false;

        template<typename T>
        struct EntityComponentAccess
        {
            static constexpr bool supported = false;
            static constexpr bool addable = false;
            static constexpr bool removable = false;
        };
    }



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



        // --- Component Setters ---
        
        std::string SetName(const std::string& name);
        Prism::Transform* AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale);
        Prism::MeshRendererComponent* AddMeshRenderer(Prism::Mesh mesh, Prism::Material material);
        Prism::SkinnedMeshRendererComponent* AddSkinnedMeshRenderer(Prism::SkinnedMesh mesh, Prism::Material material, Prism::Entity root_animator = Prism::Entity());
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
        Prism::ReflectionProbeComponent* AddReflectionProbe(const Prism::Vector3& box_extents, float blend_distance = 1.0f, uint32_t capture_resolution = 128);



        // --- Component Getters ---
        
        std::string GetName();
        Prism::Transform* GetTransform();
        Prism::MeshRendererComponent* GetMeshRenderer();
        Prism::SkinnedMeshRendererComponent* GetSkinnedMeshRenderer();
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
        Prism::ReflectionProbeComponent* GetReflectionProbe();



        // --- Component Hierarchy Getters ---

        std::vector<Prism::Transform*> GetTransformsInChildren(bool recursive = true);
        std::vector<Prism::MeshRendererComponent*> GetMeshRenderersInChildren(bool recursive = true);
        std::vector<Prism::SkinnedMeshRendererComponent*> GetSkinnedMeshRenderersInChildren(bool recursive = true);
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
        // TODO: Add for ReflectionProbe
        
        
        Prism::Transform* GetTransformInParent();
        Prism::MeshRendererComponent* GetMeshRendererInParent();
        Prism::SkinnedMeshRendererComponent* GetSkinnedMeshRendererInParent();
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
        // TODO: Add for ReflectionProbe



        // --- Component Removers ---

        void RemoveMeshRenderer();
        void RemoveSkinnedMeshRenderer();
        void RemoveRigidbody();
        void RemoveCollider();
        void RemoveCamera();
        void RemoveLight();
        void RemoveAudioListener();
        void RemoveAudioSource();
        void RemoveAnimator();
        void RemoveLineRenderer();
        void RemoveSpriteRenderer();
        void RemoveReflectionProbe();



        // --- Generic Component API ---

        // Adds a supported component using the same arguments as its named Add function.
        template<typename T, typename... Args>
        T* AddComponent(Args&&... args);

        // Returns the requested component, or nullptr if the entity does not have it.
        template<typename T>
        T* GetComponent();

        template<typename T>
        bool HasComponent();

        // Returns matching components on child entities. The entity itself is not included.
        template<typename T>
        std::vector<T*> GetComponentsInChildren(bool recursive = true);

        // Walks up the hierarchy and returns the first matching component.
        template<typename T>
        T* GetComponentInParent();

        // Removes a supported removable component.
        template<typename T>
        void RemoveComponent();



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





    // Maps public component types to the existing named Entity API.
    namespace Detail
    {
        template<>
        struct PRISM_API EntityComponentAccess<Transform>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = false;

            static Transform* Get(Entity& entity) { return entity.GetTransform(); }
            static Transform* Add(Entity& entity, const Vector3& pos, const Quaternion& rot, const Vector3& scale) { return entity.AddTransform(pos, rot, scale); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<MeshRendererComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static MeshRendererComponent* Get(Entity& entity) { return entity.GetMeshRenderer(); }
            static MeshRendererComponent* Add(Entity& entity, Mesh mesh, Material material) { return entity.AddMeshRenderer(mesh, material); }
            static void Remove(Entity& entity) { entity.RemoveMeshRenderer(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<SkinnedMeshRendererComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static SkinnedMeshRendererComponent* Get(Entity& entity) { return entity.GetSkinnedMeshRenderer(); }
            static SkinnedMeshRendererComponent* Add(Entity& entity, SkinnedMesh mesh, Material material, Entity root_animator = Entity()) { return entity.AddSkinnedMeshRenderer(mesh, material, root_animator); }
            static void Remove(Entity& entity) { entity.RemoveSkinnedMeshRenderer(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<RigidbodyComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static RigidbodyComponent* Get(Entity& entity) { return entity.GetRigidbody(); }
            static RigidbodyComponent* Add(Entity& entity, float mass) { return entity.AddRigidbody(mass); }
            static void Remove(Entity& entity) { entity.RemoveRigidbody(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<ColliderComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = false;
            static constexpr bool removable = true;

            static ColliderComponent* Get(Entity& entity) { return entity.GetCollider(); }
            static void Remove(Entity& entity) { entity.RemoveCollider(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<BoxColliderComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static BoxColliderComponent* Get(Entity& entity);
            static BoxColliderComponent* Add(Entity& entity);
            static void Remove(Entity& entity) { entity.RemoveCollider(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<SphereColliderComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static SphereColliderComponent* Get(Entity& entity);
            static SphereColliderComponent* Add(Entity& entity);
            static void Remove(Entity& entity) { entity.RemoveCollider(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<MeshColliderComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static MeshColliderComponent* Get(Entity& entity);
            static MeshColliderComponent* Add(Entity& entity);
            static void Remove(Entity& entity) { entity.RemoveCollider(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<CameraComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static CameraComponent* Get(Entity& entity) { return entity.GetCamera(); }
            static CameraComponent* Add(Entity& entity, float fov_degrees) { return entity.AddCamera(fov_degrees); }
            static void Remove(Entity& entity) { entity.RemoveCamera(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<LightComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static LightComponent* Get(Entity& entity) { return entity.GetLight(); }
            static LightComponent* Add(Entity& entity, LightType type, const Color& color) { return entity.AddLight(type, color); }
            static void Remove(Entity& entity) { entity.RemoveLight(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<AudioListenerComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static AudioListenerComponent* Get(Entity& entity) {return entity.GetAudioListener(); }
            static AudioListenerComponent* Add(Entity& entity) {return entity.AddAudioListener(); }
            static void Remove(Entity& entity)                 {entity.RemoveAudioListener(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<AudioSourceComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static AudioSourceComponent* Get(Entity& entity) { return entity.GetAudioSource(); }
            static AudioSourceComponent* Add(Entity& entity) { return entity.AddAudioSource(); }
            static void Remove(Entity& entity) { entity.RemoveAudioSource(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<AnimatorComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static AnimatorComponent* Get(Entity& entity) { return entity.GetAnimator(); }
            static AnimatorComponent* Add(Entity& entity, void* raw_skeleton, const AnimationClip& default_clip) { return entity.AddAnimator(raw_skeleton, default_clip); }
            static void Remove(Entity& entity) { entity.RemoveAnimator(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<BoneAttachmentComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = false;
            static constexpr bool removable = false;

            static BoneAttachmentComponent* Get(Entity& entity) { return entity.GetBoneAttachment(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<LineRendererComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static LineRendererComponent* Get(Entity& entity) { return entity.GetLineRenderer(); }
            static LineRendererComponent* Add(Entity& entity, Material* material = nullptr) { return entity.AddLineRenderer(material); }
            static void Remove(Entity& entity) { entity.RemoveLineRenderer(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<SpriteRendererComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static SpriteRendererComponent* Get(Entity& entity) { return entity.GetSpriteRenderer(); }
            static SpriteRendererComponent* Add(Entity& entity, Material* material = nullptr) { return entity.AddSpriteRenderer(material); }
            static void Remove(Entity& entity) { entity.RemoveSpriteRenderer(); }
        };

        template<>
        struct PRISM_API EntityComponentAccess<ReflectionProbeComponent>
        {
            static constexpr bool supported = true;
            static constexpr bool addable = true;
            static constexpr bool removable = true;

            static ReflectionProbeComponent* Get(Entity& entity) { return entity.GetReflectionProbe(); }
            static ReflectionProbeComponent* Add(Entity& entity, const Vector3& extents, float blend_distance = 1.0f, uint32_t resolution = 128) { return entity.AddReflectionProbe(extents, blend_distance, resolution); }
            static void Remove(Entity& entity) { entity.RemoveReflectionProbe(); }
        };
    }





    template<typename T, typename... Args>
    T* Entity::AddComponent(Args&&... args)
    {
        using Access = Detail::EntityComponentAccess<T>;

        if constexpr (!Access::supported) {
            static_assert(Detail::AlwaysFalse<T>, "T is not a supported built-in component type");
            return nullptr;
        }
        else if constexpr (!Access::addable) {
            static_assert(Detail::AlwaysFalse<T>, "This component cannot be added generically; use its named Entity Add function");
            return nullptr;
        }
        else {
            return Access::Add(*this, std::forward<Args>(args)...);
        }
    }



    template<typename T>
    T* Entity::GetComponent()
    {
        using Access = Detail::EntityComponentAccess<T>;

        if constexpr (!Access::supported) {
            static_assert(Detail::AlwaysFalse<T>, "T is not a supported built-in component type");
            return nullptr;
        }
        else {
            return Access::Get(*this);
        }
    }



    template<typename T>
    bool Entity::HasComponent()
    {
        return GetComponent<T>() != nullptr;
    }



    template<typename T>
    std::vector<T*> Entity::GetComponentsInChildren(bool recursive)
    {
        std::vector<T*> result;
        std::vector<Entity> children = GetChildren(recursive);

        for (Entity& child : children)
        {
            if (T* component = child.GetComponent<T>())
                result.push_back(component);
        }

        return result;
    }



    template<typename T>
    T* Entity::GetComponentInParent()
    {
        Entity current = GetParent();

        while (current.IsValid())
        {
            if (T* component = current.GetComponent<T>())
                return component;

            current = current.GetParent();
        }

        return nullptr;
    }



    template<typename T>
    void Entity::RemoveComponent()
    {
        using Access = Detail::EntityComponentAccess<T>;

        if constexpr (!Access::supported) {
            static_assert(Detail::AlwaysFalse<T>, "T is not a supported built-in component type");
        }
        else if constexpr (!Access::removable) {
            static_assert(Detail::AlwaysFalse<T>, "This component cannot be removed generically");
        }
        else {
            Access::Remove(*this);
        }
    }
    
}