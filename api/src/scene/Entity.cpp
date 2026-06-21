#include "../../include/Engine.hpp"
#include "../../include/scene/Entity.hpp"
#include "../../include/scene/Scene.hpp" // Needed for Scene::GetActive()


extern "C"
{
    #include "../../../scene/scene.h"
}


namespace Prism
{
    // ==========================================
    // Constructors
    // ==========================================

    Entity::Entity() : id(0), scene_ptr(nullptr) {}
    Entity::Entity(uint32_t entity_id, void* scene) : id(entity_id), scene_ptr(scene) {}

    // Helper macro/function to quickly build the struct
    static inline ::Entity ToCore(const Entity& e) {
        return { e.id, static_cast<::Scene*>(e.scene_ptr) };
    }
    static inline ::Matrix4 MatToCore(const Prism::Matrix4& m) {
        ::Matrix4 c_M = ::Matrix4{
            m.m0, m.m1, m.m2, m.m3,
            m.m4, m.m5, m.m6, m.m7,
            m.m8, m.m9, m.m10, m.m11,
            m.m12, m.m13, m.m14, m.m15,
        };
        return c_M;
    }



    // ==========================================
    // Core Functions
    // ==========================================

    bool Entity::IsValid() const {
        return ::Entity_IsValid(ToCore(*this));
    }
    void Entity::Destroy() {
        ::Entity_Destroy(ToCore(*this));
    }
    void Entity::SetActive(bool active) {
        ::Entity_SetActive(ToCore(*this), active);
    }

    void Entity::SetParent(Entity parent) {
        ::Entity_SetParent(ToCore(*this), ToCore(parent));
        ::BoneAttachmentComponent* attachment = ::Entity_GetBoneAttachment(ToCore(*this));
        if (attachment)
            attachment->is_active = false;
    }
    void Entity::SetParent(Entity parent, const char* bone_name, Prism::Matrix4 local_offset) {
        ::Entity_SetParent(ToCore(*this), ToCore(parent));
        int bone_idx = ::Entity_GetAnimatorBoneIndex(ToCore(parent), bone_name);

        if (bone_idx != -1)
            ::Entity_AddBoneAttachment(ToCore(*this), bone_idx, MatToCore(local_offset));
        else
            Debug_Log("Failed to attach. Parent has no animator or bone does not exist.");
    }
    Prism::Entity Entity::GetParent() {
        ::Entity raw_e = ::Entity_GetParent(ToCore(*this));        
        return Entity(raw_e.id, raw_e.scene);
    }
    void Entity::RemoveParent() {
        ::Entity_RemoveParent(ToCore(*this));
        ::BoneAttachmentComponent* attachment = ::Entity_GetBoneAttachment(ToCore(*this));
        if (attachment)
            attachment->is_active = false;
    }
    void Entity::SetTag(const std::string& tag) {
        ::Entity_SetTag(ToCore(*this), tag.c_str());
    }
    std::string Entity::GetTag() const {
        const char* c_tag = ::Entity_GetTag(ToCore(*this));
        return c_tag ? std::string(c_tag) : "";
    }
    bool Entity::CompareTag(const std::string& tag) const {
        const char* c_tag = ::Entity_GetTag(ToCore(*this));
        if (!c_tag)
            return false;
        return strcmp(c_tag, tag.c_str()) == 0;
    }
    void Entity::AddModel(Prism::Model model) {
        ::Entity_AddModel(ToCore(*this), (::Model*)model.GetRawModel());
    }

    std::vector<Prism::Entity> Entity::GetChildren(bool recursive) {
        // Create a temporary buffer (Max 1024 children to prevent stack overflow)
        const uint32_t MAX_CHILDREN = 1024;
        uint32_t buffer[MAX_CHILDREN];

        // Call the Backend
        ::Entity raw_entity = ToCore(*this);
        uint32_t found_count = ::Entity_GetChildren(raw_entity, buffer, MAX_CHILDREN, recursive);

        // Convert the raw IDs into C++ Entity objects
        std::vector<Prism::Entity> children;
        children.reserve(found_count);

        for (uint32_t i = 0; i < found_count; i++)
            children.emplace_back(Prism::Entity(buffer[i], raw_entity.scene));

        return children;
    }

    Prism::Entity Entity::FindChildByName(const std::string& name) const {
        ::Entity c_entity = ToCore(*this);
        ::Entity result = ::Entity_FindChildByName(c_entity, name.c_str());
        return Prism::Entity(result.id, this->scene_ptr);
    }

    Prism::Ray Entity::ScreenPointToRay(const Prism::Vector2& mouse_pos)
    {
        // Fetch both components
        Prism::CameraComponent* cam = this->GetCamera();
        Prism::Transform* t = this->GetTransform();

        // Safety check
        if (!cam || !t) 
            return Prism::Ray{ {0,0,0}, {0,0,0} };

        int width = Prism::Engine::GetWindowWidth();
        int height = Prism::Engine::GetWindowHeight();

        if (height == 0.0f)
            height = 1.0f;

        float aspect_ratio = (float)width / (float)height;

        // Generate the projection matrix
        ::Matrix4 c_proj = ::Matrix4Perspective(cam->fov, aspect_ratio, cam->nearZ, cam->farZ);

        Prism::Matrix4 world = t->GetWorldMatrix();
        ::Matrix4 c_world = MatToCore(world);
        
        Prism::Vector3 global_pos = t->GetGlobalPosition();
        
        
        ::Ray raw_ray = ::Camera_ScreenPointToRay(c_proj, c_world, ::Vector3{global_pos.x, global_pos.y, global_pos.z}, mouse_pos.x, mouse_pos.y, width, height);
        
        return Prism::Ray{ {raw_ray.origin.x, raw_ray.origin.y, raw_ray.origin.z}, 
                           {raw_ray.direction.x, raw_ray.direction.y, raw_ray.direction.z} };
    }



    // ==========================================
    // Component Setters
    // ==========================================

    std::string Entity::SetName(const std::string& name) { 
        ::Entity_SetName(ToCore(*this), name.c_str());
        return this->GetName();
    }
    Prism::Transform* Entity::AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale) {
        ::Entity_AddTransform(ToCore(*this), {pos.x, pos.y, pos.z}, {rot.x, rot.y, rot.z, rot.w}, {scale.x, scale.y, scale.z});
        return this->GetTransform();
    }
    Prism::RenderComponent* Entity::AddRenderable(Prism::Mesh mesh, Prism::Material material) {
        ::Entity_AddRenderable(ToCore(*this), (::Mesh*)mesh.GetRaw(), (::Material*)material.GetRaw());
        return this->GetRenderable();
    }
    Prism::CameraComponent* Entity::AddCamera(float fovDegrees) {
        float fov_radians = fovDegrees * (3.14159265f / 180.0f);
        ::Entity_AddCamera(ToCore(*this), fov_radians, 0.1f, 1000.0f);
        return this->GetCamera();
    }
    Prism::LightComponent* Entity::AddLight(Prism::LightType type, const Prism::Color& color) {
        ::Entity_AddLight(ToCore(*this), static_cast<::LightType>(type), ::Color{color.r, color.g, color.b, color.a});
        return this->GetLight();
    }
    Prism::RigidbodyComponent* Entity::AddRigidbody(float mass) {
        ::Entity_AddRigidbody(ToCore(*this), mass);
        return this->GetRigidbody();
    }
    Prism::ColliderComponent* Entity::AddColliderBox(const Prism::Vector3 extents, bool is_trigger) {
        ::Entity_AddColliderBox(ToCore(*this), {extents.x, extents.y, extents.z}, is_trigger);
        return this->GetCollider();
    }
    Prism::ColliderComponent* Entity::AddColliderBoxAuto(bool is_trigger) {
        ::Entity_AddColliderBoxAuto(ToCore(*this), is_trigger);
        return this->GetCollider();
    }
    Prism::ColliderComponent* Entity::AddColliderSphere(float radius, bool is_trigger) {
        ::Entity_AddColliderSphere(ToCore(*this), radius, is_trigger);
        return this->GetCollider();
    }
    Prism::ColliderComponent* Entity::AddColliderMesh(Prism::Mesh mesh, bool is_trigger, bool is_convex) {
        ::Entity_AddColliderMesh(ToCore(*this), (::Mesh*)mesh.GetRaw(), is_trigger, is_convex);
        return this->GetCollider();
    }
    Prism::AudioListenerComponent* Entity::AddAudioListener() {
        ::Entity_AddAudioListener(ToCore(*this));
        return this->GetAudioListener();
    }
    Prism::AudioSourceComponent* Entity::AddAudioSource() {
        ::Entity_AddAudioSource(ToCore(*this));
        return this->GetAudioSource();
    }
    Prism::AnimatorComponent* Entity::AddAnimator(void* raw_skeleton, const Prism::AnimationClip& default_clip) {
        ::Entity_AddAnimator(ToCore(*this), raw_skeleton, default_clip.GetRaw());
        return this->GetAnimator();
    }



    // ==========================================
    // Component Getters
    // ==========================================

    std::string Entity::GetName() {
        // Assuming your backend has an Entity_GetName function that returns a const char*
        const char* name = ::Entity_GetName(ToCore(*this));
        return name ? std::string(name) : std::string("Unknown");
    }
    Prism::Transform* Entity::GetTransform() {
        return reinterpret_cast<Prism::Transform*>(::Entity_GetTransform(ToCore(*this)));
    }
    Prism::RenderComponent* Entity::GetRenderable() {
        return reinterpret_cast<Prism::RenderComponent*>(::Entity_GetRenderable(ToCore(*this)));
    }
    Prism::RigidbodyComponent* Entity::GetRigidbody() {
        return reinterpret_cast<Prism::RigidbodyComponent*>(::Entity_GetRigidbody(ToCore(*this)));
    }
    Prism::ColliderComponent* Entity::GetCollider() {
        return reinterpret_cast<Prism::ColliderComponent*>(::Entity_GetCollider(ToCore(*this)));
    }
    Prism::CameraComponent* Entity::GetCamera() {
        return reinterpret_cast<Prism::CameraComponent*>(::Entity_GetCamera(ToCore(*this)));
    }
    Prism::LightComponent* Entity::GetLight() {
        return reinterpret_cast<Prism::LightComponent*>(::Entity_GetLight(ToCore(*this)));
    }
    Prism::AudioListenerComponent* Entity::GetAudioListener() {
        return reinterpret_cast<Prism::AudioListenerComponent*>(::Entity_GetAudioListener(ToCore(*this)));
    }
    Prism::AudioSourceComponent* Entity::GetAudioSource() {
        return reinterpret_cast<Prism::AudioSourceComponent*>(::Entity_GetAudioSource(ToCore(*this)));
    }
    Prism::AnimatorComponent* Entity::GetAnimator() {
        return reinterpret_cast<Prism::AnimatorComponent*>(::Entity_GetAnimator(ToCore(*this)));
    }
    Prism::BoneAttachmentComponent* Entity::GetBoneAttachment() {
        return reinterpret_cast<Prism::BoneAttachmentComponent*>(::Entity_GetBoneAttachment(ToCore(*this)));
    }



    // ==========================================
    // Component Hierarchy Getters
    // ==========================================

    std::vector<Prism::Transform*> Entity::GetTransformsInChildren(bool recursive) {
        std::vector<Prism::Transform*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::Transform* comp = child.GetTransform();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::RenderComponent*> Entity::GetRenderablesInChildren(bool recursive) {
        std::vector<Prism::RenderComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::RenderComponent* comp = child.GetRenderable();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::RigidbodyComponent*> Entity::GetRigidbodiesInChildren(bool recursive) {
        std::vector<Prism::RigidbodyComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::RigidbodyComponent* comp = child.GetRigidbody();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::ColliderComponent*> Entity::GetCollidersInChildren(bool recursive) {
        std::vector<Prism::ColliderComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::ColliderComponent* comp = child.GetCollider();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::CameraComponent*> Entity::GetCamerasInChildren(bool recursive) {
        std::vector<Prism::CameraComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::CameraComponent* comp = child.GetCamera();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::LightComponent*> Entity::GetLightsInChildren(bool recursive) {
        std::vector<Prism::LightComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::LightComponent* comp = child.GetLight();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::AudioListenerComponent*> Entity::GetAudioListenersInChildren(bool recursive) {
        std::vector<Prism::AudioListenerComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::AudioListenerComponent* comp = child.GetAudioListener();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::AudioSourceComponent*> Entity::GetAudioSourcesInChildren(bool recursive) {
        std::vector<Prism::AudioSourceComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::AudioSourceComponent* comp = child.GetAudioSource();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::AnimatorComponent*> Entity::GetAnimatorsInChildren(bool recursive) {
        std::vector<Prism::AnimatorComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::AnimatorComponent* comp = child.GetAnimator();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }

    std::vector<Prism::BoneAttachmentComponent*> Entity::GetBoneAttachmentsInChildren(bool recursive) {
        std::vector<Prism::BoneAttachmentComponent*> result;
        std::vector<Prism::Entity> children = GetChildren(recursive);

        for (Prism::Entity& child : children)
        {
            Prism::BoneAttachmentComponent* comp = child.GetBoneAttachment();
            if (comp != nullptr) result.push_back(comp);
        }
        return result;
    }





    Prism::Transform* Entity::GetTransformInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::Transform* comp = current.GetTransform();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::RenderComponent* Entity::GetRenderableInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::RenderComponent* comp = current.GetRenderable();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::RigidbodyComponent* Entity::GetRigidbodyInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::RigidbodyComponent* comp = current.GetRigidbody();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::ColliderComponent* Entity::GetColliderInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::ColliderComponent* comp = current.GetCollider();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::CameraComponent* Entity::GetCameraInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::CameraComponent* comp = current.GetCamera();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::LightComponent* Entity::GetLightInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::LightComponent* comp = current.GetLight();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::AudioListenerComponent* Entity::GetAudioListenerInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::AudioListenerComponent* comp = current.GetAudioListener();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::AudioSourceComponent* Entity::GetAudioSourceInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::AudioSourceComponent* comp = current.GetAudioSource();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::AnimatorComponent* Entity::GetAnimatorInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::AnimatorComponent* comp = current.GetAnimator();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }

    Prism::BoneAttachmentComponent* Entity::GetBoneAttachmentInParent() {
        Prism::Entity current = this->GetParent();

        // Walk up the tree until we hit the root
        while (current.IsValid())
        {
            Prism::BoneAttachmentComponent* comp = current.GetBoneAttachment();
            if (comp != nullptr) return comp;
            
            current = current.GetParent(); // Move up one level
        }
        return nullptr; // Not found in any parent
    }



    // ==========================================
    // Component Removers
    // ==========================================

    void Entity::RemoveRenderable() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_RENDER);
    }
    void Entity::RemoveRigidbody() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_RIGIDBODY);
    }
    void Entity::RemoveCollider() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_COLLIDER);
    }
    void Entity::RemoveCamera() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_CAMERA);
    }
    void Entity::RemoveLight() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_LIGHT);
    }
    void Entity::RemoveAudioListener() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_AUDIO_LISTENER);
    }
    void Entity::RemoveAudioSource() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_AUDIO_SOURCE);
    }
    void Entity::RemoveAnimator() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_ANIMATOR);
    }



    // ==========================================
    // Utility
    // ==========================================
    
    Entity Entity::Find(const std::string& name)
    {
        // Grab the active scene
        void* active_scene_ptr = Scene::GetActive().GetRaw();
        
        if (!active_scene_ptr)
        {
            Debug_Warn("No active scene");
            return Entity(); // Returns invalid entity
        }

        // Query the backend
        ::Entity raw_e = ::Scene_GetEntity(static_cast<::Scene*>(active_scene_ptr), name.c_str());
        
        // Return a API Entity
        return Entity(raw_e.id, raw_e.scene);
    }
}