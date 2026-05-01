#pragma once

#include "../core/Log.hpp"
#include "../core/Math.hpp"
#include "../AssetManager.hpp"
#include "Entity.hpp"



namespace Prism
{
    // Enum for built in and custom collision layers
    enum CollisionLayer
    {
        COLLISION_LAYER_NONE      = 0,
        COLLISION_LAYER_DEFAULT   = (1 << 0),  // Standard colliders
        COLLISION_LAYER_TRIGGER   = (1 << 1),  // Invisible triggers



        // User defined layers

        COLLISION_LAYER_USER_1    = (1 << 2),
        COLLISION_LAYER_USER_2    = (1 << 3),
        COLLISION_LAYER_USER_3    = (1 << 4),
        COLLISION_LAYER_USER_4    = (1 << 5),
        COLLISION_LAYER_USER_5    = (1 << 6),
        COLLISION_LAYER_USER_6    = (1 << 7),
        COLLISION_LAYER_USER_7    = (1 << 8),
        COLLISION_LAYER_USER_8    = (1 << 9),
        COLLISION_LAYER_USER_9    = (1 << 10),
        COLLISION_LAYER_USER_10   = (1 << 11),
        COLLISION_LAYER_USER_11   = (1 << 12),
        COLLISION_LAYER_USER_12   = (1 << 13),
        COLLISION_LAYER_USER_13   = (1 << 14),



        // Collision Masks

        COLLISION_MASK_NONE       = 0,
        COLLISION_MASK_ALL        = -1
    };



    // Enum for collider types
    enum ColliderType
    {
        COLLIDER_BOX,
        COLLIDER_SPHERE,
        COLLIDER_MESH
    };



    // ==========================================
    // Transform Wrapper
    // ==========================================

    struct Transform
    {
    public:
        Prism::Vector3 local_position;
        Prism::Vector3 local_rotation_euler;
        Prism::Quaternion local_rotation;
        Prism::Vector3 local_scale;

    private:
        Prism::Matrix4 world_matrix;
        uint32_t parent_id;
        uint32_t child_id;
        uint32_t next_sib_id;
        uint32_t prev_sib_id;
        bool is_dirty;

    public:
        void SetLocalPosition(const Prism::Vector3& pos);
        void SetLocalRotationEuler(const Prism::Vector3& euler);
        void SetLocalRotation(const Prism::Quaternion& rot);
        void SetLocalScale(const Prism::Vector3& scale);

        void Translate(const Prism::Vector3& translation);
        void RotateEuler(const Prism::Vector3& euler_addition);

        Prism::Vector3 GetLocalPosition();
        Prism::Vector3 GetGlobalPosition();
        Prism::Vector3 GetGlobalScale();
        Prism::Quaternion GetGlobalRotation();

        Prism::Vector3 GetForwardVector();
        Prism::Vector3 GetRightVector();
        Prism::Vector3 GetUpVector();
    };



    // ==========================================
    // Rigidbody Wrapper
    // ==========================================
    
    struct RigidbodyComponent
    {
        Prism::Entity owner;
        float mass;
        bool is_kinematic;
        bool use_gravity;
        float linear_drag;
        float angular_drag;


        void SetGravity(bool use_gravity);
        void SetKinematic(bool kinematic);
    };



    // ==========================================
    // Collider Wrapper
    // ==========================================

    struct ColliderComponent
    {
        Prism::Entity owner;
        int type;
        bool is_trigger;
        CollisionLayer collision_layer;
        int collision_mask;
        Vector3 extents;
        float radius;
        uint32_t mesh_id;
        Vector3 mesh_scale;


        void SetLayerAndMask(CollisionLayer layer, int mask);
        void SetBoxExtents(const Prism::Vector3& new_extents);
        void SetSphereRadius(float new_radius);
        void SetMeshScale(const Prism::Vector3& new_scale);
    };

    // (Include CameraComponent, PointLightComponent, etc., if they need specific methods)
}