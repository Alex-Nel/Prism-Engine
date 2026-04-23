#pragma once

#include "../core/Log.hpp"
#include "../core/Math.hpp"
#include "../AssetManager.hpp"

extern "C"
{
    #include "scene/scene.h"
}

namespace Prism
{
    // ==========================================
    // Transform Wrapper
    // ==========================================
    struct Transform : public ::Transform
    {    
        void SetLocalPosition(const Prism::Vector3& pos) {
            ::Transform_SetLocalPosition(this, pos);
        }
        void SetLocalRotationEuler(const Prism::Vector3& euler) {
            ::Transform_SetLocalRotationEuler(this, euler);
        }
        void SetLocalRotation(const Prism::Quaternion& rot) {
            ::Transform_SetLocalRotation(this, rot);
        }
        void SetLocalScale(const Prism::Vector3& scale) {
            ::Transform_SetLocalScale(this, scale);
        }

        void Translate(const Prism::Vector3& translation) {
            ::Transform_Translate(this, translation);
        }
        void RotateEuler(const Prism::Vector3& euler_addition) {
            ::Transform_RotateEuler(this, euler_addition);
        }

        Prism::Vector3 GetLocalPosition() {
            return ::Transform_GetLocalPosition(this);
        }
        Prism::Vector3 GetGlobalPosition() {
            return ::Transform_GetGlobalPosition(this);
        }
        Prism::Vector3 GetGlobalScale() {
            return ::Transform_GetGlobalScale(this);
        }
        Prism::Quaternion GetGlobalRotation() {
            return ::Transform_GetGlobalRotation(this);
        }

        Prism::Vector3 GetForwardVector() {
            return ::Transform_GetForwardVector(this);
        }
        Prism::Vector3 GetRightVector() {
            return ::Transform_GetRightVector(this);
        }
        Prism::Vector3 GetUpVector() {
            return ::Transform_GetUpVector(this);
        }
    };



    // ==========================================
    // Rigidbody Wrapper
    // ==========================================
    // Note: To use Entity functions inside a component wrapper, we pass the raw ID.
    struct RigidbodyComponent : public ::RigidbodyComponent
    {    
        void SetGravity(bool use_gravity) { 
            ::Rigidbody_SetGravity(this->owner, use_gravity); 
        }
        
        void SetKinematic(bool kinematic) { 
            ::Rigidbody_SetKinematic(this->owner, kinematic); 
        }
    };



    // ==========================================
    // Collider Wrapper
    // ==========================================
    struct ColliderComponent : public ::ColliderComponent
    {    
        void SetLayerAndMask(::CollisionLayer layer, int mask) {
            ::Collider_SetLayerAndMask(this->owner, layer, mask);
        }

        void SetBoxExtents(const Prism::Vector3& new_extents) {
            if (type != COLLIDER_BOX) {
                Debug_Warn("Attempted to set box extents on a non-box collider!");
                return;
            }
            ::Collider_SetBoxExtents(this->owner, new_extents);
        }

        void SetSphereRadius(float new_radius) {
            if (type != COLLIDER_SPHERE) {
                Debug_Warn("Attempted to set sphere radius on a non-sphere collider!");
                return;
            }
            ::Collider_SetSphereRadius(this->owner, new_radius);
        }

        void SetMeshScale(const Prism::Vector3& new_scale) {
            if (type != COLLIDER_MESH) {
                Debug_Warn("Attempted to set mesh scale on a non-mesh collider!");
                return;
            }
            ::Collider_SetMeshScale(this->owner, new_scale);
        }
    };

    // (You can also wrap CameraComponent, PointLightComponent, etc., if they need specific methods)
}