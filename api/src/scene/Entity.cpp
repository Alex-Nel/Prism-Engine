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
    }
    void Entity::RemoveParent() {
        ::Entity_RemoveParent(ToCore(*this));
    }
    void Entity::AddModel(Prism::Model model) {
        ::Entity_AddModel(ToCore(*this), (::Model*)model.GetRawModel());
    }



    // ==========================================
    // Component Setters
    // ==========================================
    void Entity::SetName(const std::string& name) { 
        ::Entity_SetName(ToCore(*this), name.c_str()); 
    }
    void Entity::AddTransform(const Vector3& pos, const Quaternion& rot, const Vector3& scale) {
        ::Entity_AddTransform(ToCore(*this), {pos.x, pos.y, pos.z}, {rot.x, rot.y, rot.z, rot.w}, {scale.x, scale.y, scale.z});
    }
    void Entity::AddRenderable(Prism::Mesh mesh, Prism::Material material) {
        ::Entity_AddRenderable(ToCore(*this), (::Mesh*)mesh.GetRaw(), (::Material*)material.GetRaw());
    }
    void Entity::AddCamera(float fovDegrees) {
        float fov_radians = fovDegrees * (3.14159265f / 180.0f);
        ::Entity_AddCamera(ToCore(*this), fov_radians, 0.1f, 1000.0f);
    }
    void Entity::AddPointLight(const Prism::Vector3& color) {
        ::Entity_AddPointLight(ToCore(*this), {color.x, color.y, color.z}, 1.0f, 1.0f, 0.09f, 0.032f);
    }
    void Entity::AddRigidbody(float mass) {
        ::Entity_AddRigidbody(ToCore(*this), mass);
    }
    void Entity::AddColliderBox(const Prism::Vector3& extents, bool is_trigger) {
        ::Entity_AddColliderBox(ToCore(*this), {extents.x, extents.y, extents.z}, is_trigger);
    }
    void Entity::AddColliderBoxAuto(bool is_trigger) {
        ::Entity_AddColliderBoxAuto(ToCore(*this), is_trigger);
    }
    void Entity::AddColliderSphere(float radius, bool is_trigger) {
        ::Entity_AddColliderSphere(ToCore(*this), radius, is_trigger);
    }
    void Entity::AddColliderMesh(Prism::Mesh mesh, bool is_trigger, bool is_convex) {
        ::Entity_AddColliderMesh(ToCore(*this), (::Mesh*)mesh.GetRaw(), is_trigger, is_convex);
    }
    void Entity::AddAudioListener() {
        ::Entity_AddAudioListener(ToCore(*this));
    }
    void Entity::AddAudioSource() {
        ::Entity_AddAudioSource(ToCore(*this));
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
    Prism::PointLightComponent* Entity::GetPointLight() {
        return reinterpret_cast<Prism::PointLightComponent*>(::Entity_GetPointLight(ToCore(*this)));
    }
    Prism::AudioListenerComponent* Entity::GetAudioListener() {
        return reinterpret_cast<Prism::AudioListenerComponent*>(::Entity_GetAudioListener(ToCore(*this)));
    }
    Prism::AudioSourceComponent* Entity::GetAudioSource() {
        return reinterpret_cast<Prism::AudioSourceComponent*>(::Entity_GetAudioSource(ToCore(*this)));
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
    void Entity::RemovePointLight() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_POINT_LIGHT);
    }
    void Entity::RemoveAudioListener() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_AUDIO_LISTENER);
    }
    void Entity::RemoveAudioSource() {
        ::Entity_RemoveComponent(ToCore(*this), COMPONENT_AUDIO_SOURCE);
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