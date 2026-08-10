#pragma once

#include "Entity.hpp"
#include "../core/Log.hpp"
#include "../core/Math.hpp"
#include "../PrismAPI.hpp"
#include <cstdint>


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



    // Defines what the camera wipes before drawing
    enum CameraClearFlags
    {
        CLEAR_COLOR_AND_DEPTH = 0,
        CLEAR_DEPTH_ONLY = 1,
        CLEAR_NONE = 2
    };



    // ==========================================
    // Transform Wrapper
    // ==========================================

    struct PRISM_API Transform
    {
    public:
        Prism::Entity entity; // The Entity that this component is attached to
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

        void SetGlobalPosition(const Prism::Vector3& pos);
        void SetGlobalRotationEuler(const Prism::Vector3& euler);
        void SetGlobalRotation(const Prism::Quaternion& rot);
        void SetGlobalScale(const Prism::Vector3& scale);

        void Translate(const Prism::Vector3& translation);
        void RotateEuler(const Prism::Vector3& euler_addition);

        Prism::Vector3 GetLocalPosition();
        Prism::Vector3 GetGlobalPosition();
        Prism::Vector3 GetGlobalScale();
        Prism::Quaternion GetGlobalRotation();
        Prism::Matrix4 GetWorldMatrix();

        Prism::Vector3 GetForwardVector();
        Prism::Vector3 GetRightVector();
        Prism::Vector3 GetUpVector();
    };



    // ==========================================
    // Point Light Component Wrapper
    // ==========================================

    struct PRISM_API LightComponent
    {
        Prism::Entity entity; // The Entity that this component is attached to
        bool is_active;
        LightType type;
        Prism::Color color;
        float intensity;

        float ambient_strength;
        
        float constant;
        float linear;
        float quadratic;

        float inner_cut_off;
        float outer_cut_off;

        float shadow_box_size;

        // Cascaded shadow maps (directional lights only).
        uint8_t shadow_cascade_count;
        float shadow_max_distance;
        float cascade_split_lambda;
        float cascade_blend_fraction;

        bool casts_shadows;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }


        void SetType(LightType type);
        void SetColor(const Prism::Color& color);
        void SetIntensity(float intensity);
        void SetAmbientStrength(float ambient_strength);
        void SetAttenuation(float constant, float linear, float quadratic);
        void SetSpotAngles(float inner_cutoff_degrees, float outer_cutoff_degrees);

        void SetShadowBoxSize(float half_extent);
        float GetShadowBoxSize() const;

        void SetCascadedShadows(uint8_t cascade_count, float max_distance, float split_lambda = 0.5f, float blend_fraction = 0.12f);
        void SetCascadeBlendFraction(float fraction) { this->cascade_blend_fraction = fraction; }
        void SetCastsShadows(bool casts_shadows);
        void DisableCascadedShadows();
        uint8_t GetShadowCascadeCount() const;
        float GetShadowMaxDistance() const; 
        float GetCascadeSplitLambda() const;
        float GetCascadeBlendFraction() const;
    };



    // ==========================================
    // Mesh Render Component Wrapper
    // ==========================================

    struct PRISM_API MeshRendererComponent
    {
        Prism::Entity entity; // The Entity that this component is attached to
        bool is_active;
        void* raw_mesh_ptr;
        void* raw_material_ptr;
        uint32_t layer_mask;
        bool casts_shadows;
        bool receives_shadows;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }

        void SetMaterial(Prism::Material material);

        void SetLayerMask(uint8_t mask);
        void SetCastsShadow(bool casts_shadow);
        void SetReceivesShadow(bool receives_shadow);
    };



    // ==========================================
    // Skinned Mesh Render Component Wrapper
    // ==========================================

    struct PRISM_API SkinnedMeshRendererComponent
    {
        Prism::Entity entity; // The Entity that this component is attached to
        bool is_active;
        void* raw_mesh_ptr;
        void* raw_material_ptr;
        uint32_t layer_mask;
        bool casts_shadows;
        bool receives_shadows;
        Prism::Entity root_animator;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }

        void SetMaterial(Prism::Material material);

        void SetLayerMask(uint8_t mask);
        void SetCastsShadow(bool casts_shadow);
        void SetReceivesShadow(bool receives_shadow);
    };



    // ==========================================
    // Camera Component Wrapper
    // ==========================================

    struct PRISM_API CameraComponent
    {
        Prism::Entity entity; // The Entity that this component is attached to
        bool is_active;
        float fov;
        float nearZ;
        float farZ;
        Prism::Matrix4 projection_matrix; 
        bool is_dirty;
        uint32_t culling_masks;
        int render_order;
        CameraClearFlags clear_flags;
        uint32_t viewport_x;
        uint32_t viewport_y;
        uint32_t viewport_width;
        uint32_t viewport_height;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }

        Prism::Ray ScreenPointToRay(const Prism::Vector2& screenPoint) const;
        Prism::Vector2 WorldToScreenPoint(const Prism::Vector3& worldPosition) const;

        void SetCullingMask(uint32_t layer_index);
        void AddLayerToMask(uint8_t layer_index);
        void RemoveLayerFromMask(uint8_t layer_index);
        void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
        void SetFOV(float fov);
    };



    // ==========================================
    // Rigidbody Wrapper
    // ==========================================
    
    struct PRISM_API RigidbodyComponent
    {
        Prism::Entity owner; // The Entity that this component is attached to
        bool is_active;
        float mass;
        float linear_drag;
        float angular_drag;
        bool use_gravity;
        bool is_kinematic;

        bool freeze_rot_x;
        bool freeze_rot_y;
        bool freeze_rot_z;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }


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
    public:
        Prism::Entity owner; // The Entity that this component is attached to
        bool is_active;
        int type;
        bool is_trigger;
        bool is_convex;
    private:
        void* physics_handle;
        void* raw_mesh_ptr;
    public:
        Prism::Vector3 extents;
        float radius;
        Prism::Vector3 mesh_scale;
    private:
        uint32_t touching_entities[MAX_COLLISION_OVERLAPS];
        uint32_t touching_count;

        CollisionLayer collision_layer;
        int collision_mask;

    public:
        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }

        void SetLayerAndMask(CollisionLayer layer, int mask);
        void SetConvex(bool is_convex);
    };



    struct PRISM_API BoxColliderComponent : public ColliderComponent
    {
        void SetBoxExtents(const Prism::Vector3& new_extents);
    };



    struct PRISM_API SphereColliderComponent : public ColliderComponent
    {
        void SetSphereRadius(float new_radius);
    };



    struct PRISM_API MeshColliderComponent : public ColliderComponent
    {
        void SetMeshScale(const Prism::Vector3& new_scale);
    };



    // ==========================================
    // Audio Listener Wrapper
    // ==========================================

    struct PRISM_API AudioListenerComponent 
    {
        Prism::Entity entity; // The Entity that this component is attached to
        bool is_active = true;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }
    };



    // ==========================================
    // Audio Source Wrapper
    // ==========================================

    struct PRISM_API AudioSourceComponent 
    {
        Prism::Entity entity;  // The Entity that this component is attached to
        bool is_active = true;
        Prism::AudioClip clip; // The loaded asset
        
        float volume = 1.0f;
        float pitch = 1.0f;
        
        bool loop = false;
        bool play_on_awake = true;
        bool is_playing = false;
        
        // 3D Settings
        bool is_spatial = true;
        float min_distance = 1.0f;
        float max_distance = 50.0f;


        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }

        void SetVolume(float vol) { this->volume = vol; }
        void SetPitch(float p) { this->pitch = p; }
        void SetLoop(bool l) { this->loop = l; }
        void SetSpatial(bool spatial) { this->is_spatial = spatial; }
        void SetDistances(float min_dist, float max_dist) { this->min_distance = min_dist; this->max_distance = max_dist; }

        // Helper functions so the user doesn't have to manually toggle booleans
        
        void Play() { is_playing = true; }
        void Stop() { is_playing = false; }
    };



    // ==========================================
    // Animator Wrapper
    // ==========================================

    struct PRISM_API AnimatorComponent
    {
    public:
        Prism::Entity entity; // The Entity that this component is attached to
        bool is_active;
        
        void* raw_skeleton;
        void* raw_current_clip;
        
        float current_time_ticks;
        bool is_playing;
        float playback_speed;

    private:
        Prism::Matrix4 final_bone_matrices[256];


    public:
        void SetActive(bool active) {
            this->is_active = active;
        }
        bool IsActive() const {
            return this->is_active;
        }


        // Animation Controls

        void Play() {
            this->is_playing = true;
        }
        void Pause() {
            this->is_playing = false;
        }
        void Stop() { 
            this->is_playing = false; 
            this->current_time_ticks = 0.0f; 
        }
        
        void SetPlaybackSpeed(float speed) {
            this->playback_speed = speed;
        }
        
        // Swap animations
        void SetClip(const Prism::AnimationClip& clip) {
            this->raw_current_clip = clip.GetRaw();
            this->current_time_ticks = 0.0f; // Reset timeline
        }
    };



    // ==========================================
    // Bone Attachment Wrapper
    // ==========================================

    struct PRISM_API BoneAttachmentComponent
    {
    public:
        Prism::Entity owner;
        bool is_active;
    private:
        int target_bone_index;
        Prism::Matrix4 local_offset;
        uint32_t target_animator_id;

    public:
        void SetActive(bool active) {
            this->is_active = active;
        }
        bool IsActive() const {
            return this->is_active;
        }
        void SetOffset(const Prism::Matrix4& offset) {
            this->local_offset = offset;
        }
    };



    // ==========================================
    // Line Renderer Wrapper
    // ==========================================
    
    #define MAX_LINE_POINTS 1024

    struct PRISM_API LineRendererComponent
    {
    public:
        Prism::Entity entity;
        bool is_active;
        Prism::Vector3 points[MAX_LINE_POINTS];
        uint32_t point_count;

        float start_thickness;
        float end_thickness;
        Color color;
        bool is_loop;
        bool use_world_space;
    private:
        Mesh* dynamic_mesh;
        Material* material;

    public:
        void AddPoint(const Prism::Vector3& point);
        void SetPoint(uint32_t index, const Prism::Vector3& point);
        void SetPoints(const std::vector<Prism::Vector3>& points);

        uint32_t GetPointCount();
        Prism::Vector3 GetPoint(uint32_t index) const;
        std::vector<Prism::Vector3> GetPoints() const;
        
        void ClearPoints();

        void SetThickness(float startThickness, float endThickness);
        void SetThickness(float thickness);
        void SetColor(const Prism::Color& color);
        void SetUseWorldSpace(bool UseWorldSpace);
        bool GetUseWorldSpace() const;
        void SetLoop(bool isLoop);
    };



    // ==========================================
    // Sprite Renderer Wrapper
    // ==========================================

    struct PRISM_API SpriteRendererComponent
    {
    public:
        Prism::Entity entity;
        bool is_active;
        Prism::Color color;

    public:
        void SetColor(const Prism::Color& color);
        Prism::Color GetColor() const;
    };



    // ==========================================
    // Local Reflection / Irradiance Probe
    // ==========================================

    struct PRISM_API ReflectionProbeComponent
    {
    public:
        Prism::Entity entity;
        bool is_active;
        Prism::Vector3 box_extents;
        float blend_distance;
        int32_t priority;
        uint32_t capture_resolution;
        uint32_t revision;
    private:
        bool dirty;
        bool captured;

    public:
        void SetActive(bool active) { this->is_active = active; }
        bool IsActive() const { return this->is_active; }
        void SetBoxExtents(const Prism::Vector3& extents);
        void SetBlendDistance(float distance);
        void SetPriority(int32_t new_priority);
        void SetCaptureResolution(uint32_t resolution);
        void MarkDirty();
    };

}