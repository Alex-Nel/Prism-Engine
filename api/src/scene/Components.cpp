#include "../../include/scene/Components.hpp"
#include "../../include/scene/Scene.hpp"


extern "C"
{
    #include "../../../scene/scene.h"
}


namespace Prism
{
    // ==========================================
    // Transform Implementation
    // ==========================================

    void Transform::SetLocalPosition(const Prism::Vector3& pos) {
        ::Transform_SetLocalPosition(reinterpret_cast<::Transform*>(this), {pos.x, pos.y, pos.z});
    }
    void Transform::SetLocalRotationEuler(const Prism::Vector3& euler) {
        ::Transform_SetLocalRotationEuler(reinterpret_cast<::Transform*>(this), {euler.x, euler.y, euler.z});
    }
    void Transform::SetLocalRotation(const Prism::Quaternion& rot) {
        ::Transform_SetLocalRotation(reinterpret_cast<::Transform*>(this), {rot.x, rot.y, rot.z, rot.w});
    }
    void Transform::SetLocalScale(const Prism::Vector3& scale) {
        ::Transform_SetLocalScale(reinterpret_cast<::Transform*>(this), {scale.x, scale.y, scale.z});
    }
    void Transform::Translate(const Prism::Vector3& translation) {
        ::Transform_Translate(reinterpret_cast<::Transform*>(this), {translation.x, translation.y, translation.z});
    }
    void Transform::RotateEuler(const Prism::Vector3& euler_addition) {
        ::Transform_RotateEuler(reinterpret_cast<::Transform*>(this), {euler_addition.x, euler_addition.y, euler_addition.z});
    }

    Prism::Vector3 Transform::GetLocalPosition() {
        ::Vector3 raw = ::Transform_GetLocalPosition(reinterpret_cast<::Transform*>(this));
        return Prism::Vector3(raw.x, raw.y, raw.z);
    }
    Prism::Vector3 Transform::GetGlobalPosition() {
        ::Vector3 raw = ::Transform_GetGlobalPosition(reinterpret_cast<::Transform*>(this));
        return Prism::Vector3(raw.x, raw.y, raw.z);
    }
    Prism::Vector3 Transform::GetGlobalScale() {
        ::Vector3 raw = ::Transform_GetGlobalScale(reinterpret_cast<::Transform*>(this));
        return Prism::Vector3(raw.x, raw.y, raw.z);
    }
    Prism::Quaternion Transform::GetGlobalRotation() {
        ::Quaternion raw = ::Transform_GetGlobalRotation(reinterpret_cast<::Transform*>(this));
        return Prism::Quaternion(raw.x, raw.y, raw.z, raw.w);
    }
    Prism::Matrix4 Transform::GetWorldMatrix() {
        return this->world_matrix;
    }

    Prism::Vector3 Transform::GetForwardVector() {
        ::Vector3 raw = ::Transform_GetForwardVector(reinterpret_cast<::Transform*>(this));
        return Prism::Vector3(raw.x, raw.y, raw.z);
    }
    Prism::Vector3 Transform::GetRightVector() {
        ::Vector3 raw = ::Transform_GetRightVector(reinterpret_cast<::Transform*>(this));
        return Prism::Vector3(raw.x, raw.y, raw.z);
    }
    Prism::Vector3 Transform::GetUpVector() {
        ::Vector3 raw = ::Transform_GetUpVector(reinterpret_cast<::Transform*>(this));
        return Prism::Vector3(raw.x, raw.y, raw.z);
    }



    // ==========================================
    // Rigidbody Implementation
    // ==========================================

    void RenderComponent::SetMaterial(Prism::Material material) {
        this->raw_material_ptr = material.GetRaw();
    }



    // ==========================================
    // Rigidbody Implementation
    // ==========================================
    
    void RigidbodyComponent::SetGravity(bool use_gravity) { 
        // Reconstruct the ::Entity from Prism::Entity
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetGravity(raw_e, use_gravity); 
    }
    
    void RigidbodyComponent::SetKinematic(bool kinematic) { 
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetKinematic(raw_e, kinematic); 
    }

    void RigidbodyComponent::SetLinearVelocity(Prism::Vector3& velocity) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetLinearVelocity(raw_e, ::Vector3{velocity.x, velocity.y, velocity.z}); 
    }

    void RigidbodyComponent::MovePosition(const Prism::Vector3& position) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_MovePosition(raw_e, ::Vector3{position.x, position.y, position.z});
    }



    // ==========================================
    // Collider Implementation
    // ==========================================
    
    void ColliderComponent::SetLayerAndMask(CollisionLayer layer, int mask) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetLayerAndMask(raw_e, static_cast<::CollisionLayer>(layer), mask);
    }

    void ColliderComponent::SetBoxExtents(const Prism::Vector3& new_extents) {
        if (type != COLLIDER_BOX)
        {
            Debug_Warning("Attempted to set box extents on a non-box collider!");
            return;
        }
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetBoxExtents(raw_e, {new_extents.x, new_extents.y, new_extents.z});
    }

    void ColliderComponent::SetSphereRadius(float new_radius) {
        if (type != COLLIDER_SPHERE)
        {
            Debug_Warning("Attempted to set sphere radius on a non-sphere collider!");
            return;
        }
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetSphereRadius(raw_e, new_radius);
    }

    void ColliderComponent::SetMeshScale(const Prism::Vector3& new_scale) {
        if (type != COLLIDER_MESH)
        {
            Debug_Warning("Attempted to set mesh scale on a non-mesh collider!");
            return;
        }
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetMeshScale(raw_e, {new_scale.x, new_scale.y, new_scale.z});
    }

    void ColliderComponent::SetConvex(bool is_convex)
    {
        if (type != COLLIDER_MESH)
        {
            Debug_Warning("Only mesh colliders can be convex");
            return;
        }

        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetConvex(raw_e, is_convex);
    }
}