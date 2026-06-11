#pragma once

#include "../core/Log.hpp"
#include "../core/Math.hpp"
#include "Entity.hpp"
#include "../PrismAPI.hpp"


#define MAX_MATERIAL_SLOTS 256


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

    struct PRISM_API Transform
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
    // Camera Component Wrapper
    // ==========================================

    struct PRISM_API CameraComponent
    {
        float fov;
        float nearZ;
        float farZ;
        Prism::Matrix4 projection_matrix; 
        bool is_dirty; 
    };



    // ==========================================
    // Point Light Component Wrapper
    // ==========================================

    struct PRISM_API PointLightComponent
    {
        Prism::Color color;
        float intensity;
        
        float constant;  
        float linear;    
        float quadratic; 
    };



    // ==========================================
    // Render Component Wrapper
    // ==========================================

    struct PRISM_API RenderComponent
    {
        void* raw_mesh_ptr;
        void* raw_material_ptr;

        void SetMaterial(Prism::Material material);
    };



    // ==========================================
    // Rigidbody Wrapper
    // ==========================================
    
    struct PRISM_API RigidbodyComponent
    {
        Prism::Entity owner;
        float mass;
        float linear_drag;
        float angular_drag;
        bool use_gravity;
        bool is_kinematic;

        bool freeze_rot_x;
        bool freeze_rot_y;
        bool freeze_rot_z;


        void SetGravity(bool use_gravity);
        void SetKinematic(bool kinematic);
        void SetLinearVelocity(Prism::Vector3& velocity);
        void MovePosition(const Prism::Vector3& position);
    };



    // ==========================================
    // Collider Wrapper
    // ==========================================

    #define MAX_COLLISION_OVERLAPS 16

    struct PRISM_API ColliderComponent
    {
        Prism::Entity owner;
        int type;
        bool is_trigger;
        bool is_convex;
        void* physics_handle;
        void* raw_mesh_ptr;
        Prism::Vector3 extents;
        float radius;
        Prism::Vector3 mesh_scale;

        uint32_t touching_entities[MAX_COLLISION_OVERLAPS];
        uint32_t touching_count;

        uint32_t touching_last_frame[MAX_COLLISION_OVERLAPS];
        uint32_t touching_last_count;

        CollisionLayer collision_layer;
        int collision_mask;


        void SetLayerAndMask(CollisionLayer layer, int mask);
        void SetBoxExtents(const Prism::Vector3& new_extents);
        void SetSphereRadius(float new_radius);
        void SetMeshScale(const Prism::Vector3& new_scale);
        void SetConvex(bool is_convex);
    };



    // ==========================================
    // Audio Listener Wrapper
    // ==========================================

    struct PRISM_API AudioListenerComponent 
    {
        bool active = true;
    };



    // ==========================================
    // Audio Source Wrapper
    // ==========================================

    struct PRISM_API AudioSourceComponent 
    {
        Prism::AudioClip clip; // The loaded asset
        
        float volume = 1.0f;
        float pitch = 1.0f;
        
        bool loop = false;
        bool playOnAwake = true;
        bool isPlaying = false;
        
        // 3D Settings
        bool isSpatial = true;
        float minDistance = 1.0f;
        float maxDistance = 50.0f;

        // Helper functions so the user doesn't have to manually toggle booleans
        void Play() { isPlaying = true; }
        void Stop() { isPlaying = false; }
    };

    
    
}