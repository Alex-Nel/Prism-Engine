#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../core/mathCore.h"
#include "../core/timeCore.h"
#include "../core/log.h"
#include "../assets/asset_manager.h"
#include "physicsBridge.h"

#define MAX_ENTITIES 4096
#define MAX_NAME_LENGTH 64
#define MAX_SCRIPTS_PER_ENTITY 64


typedef struct Scene Scene;

// Struct for an entity
// Contains it's ID and pointer to the scene it's in
typedef struct Entity
{
    uint32_t id;
    Scene* scene;
} Entity;

// ID for an invalid entity
#define ENTITY_NONE (uint32_t) 0xFFFFFFFF





// Enum for all Component Masks
typedef enum
{
    COMPONENT_NONE        = 0,
    COMPONENT_NAME        = 1 << 0,
    COMPONENT_TRANSFORM   = 1 << 1,
    COMPONENT_RENDER      = 1 << 2,
    COMPONENT_CAMERA      = 1 << 3,
    COMPONENT_POINT_LIGHT = 1 << 4,
    COMPONENT_COLLIDER    = 1 << 5,
    COMPONENT_RIGIDBODY   = 1 << 6,
    COMPONENT_SCRIPT      = 1 << 7
} ComponentMask;





// --- Function pointers for custom scripts ---

// On Update function
typedef void (*ScriptUpdateFunc)(Entity entity, float dt, void* instance_data);
// On Start function
typedef void (*ScriptStartFunc)(Entity entity, void* instance_data);
// On Destroy function
typedef void (*ScriptDestroyFunc)(Entity entity, void* instance_data);
// On Enable function
typedef void (*ScriptEnableFunc)(Entity entity, void* instance_data);
// On Disable function
typedef void (*ScriptDisableFunc)(Entity entity, void* instance_data);





// -------------------------------- //
// --- All Component Structures --- //
// -------------------------------- //


typedef struct NameComponent
{
    char name[MAX_NAME_LENGTH];
} NameComponent;


// Transform: an entities positional parts within the scene
typedef struct Transform
{
    Vector3 local_position;
    Vector3 local_rotation_euler;
    Quaternion local_rotation;
    Vector3 local_scale;

    Matrix4 world_matrix; 

    uint32_t parent_id;
    uint32_t first_child_id;
    uint32_t next_sibling_id;
    uint32_t prev_sibling_id;
    
    bool is_dirty; 
} Transform;


// Component that holds rendering information
typedef struct RenderComponent
{
    uint32_t mesh_id;
    uint32_t material_id;
} RenderComponent;


// Camera component for rendering
typedef struct CameraComponent
{
    float fov;
    float nearZ;
    float farZ;
    Matrix4 projection_matrix; // Projection matrix is cached to prevent recalculations
    bool is_dirty; 
} CameraComponent;


// Point light component
typedef struct PointLightComponent
{
    Vector3 color;
    float intensity;
    
    // Attenuation (Falloff) variables:
    float constant;     // usually 1.0f
    float linear;       // smaller means light travels further
    float quadratic;    // smaller means light travels further
} PointLightComponent;


// Collider Types
typedef enum ColliderType
{
    COLLIDER_BOX,
    COLLIDER_SPHERE,
    COLLIDER_MESH
} ColliderType;


// The Collider Unified Component
typedef struct ColliderComponent
{
    ColliderType type;
    bool is_trigger;
    
    void* physics_handle;
} ColliderComponent;


// Rigidbody component and variables
typedef struct RigidbodyComponent
{
    float mass;
    float linear_drag;
    float angular_drag;
    bool use_gravity;
    bool is_kinematic;

    bool freeze_rot_x;
    bool freeze_rot_y;
    bool freeze_rot_z;
} RigidbodyComponent;





// Instance of a custom script and all special functions
typedef struct ScriptInstance
{
    void* instance_data;
    ScriptStartFunc OnStart;
    ScriptUpdateFunc OnUpdate;
    ScriptDestroyFunc OnDestroy;
    ScriptEnableFunc OnEnable;
    ScriptDisableFunc OnDisable;
    bool has_started;
} ScriptInstance;


// Custom scripting component
typedef struct ScriptComponent
{
    ScriptInstance instances[MAX_SCRIPTS_PER_ENTITY];
    uint32_t count;
} ScriptComponent;


// --- The Scene Struct ---
typedef struct Scene
{
    uint32_t component_masks[MAX_ENTITIES];

    bool is_active_self[MAX_ENTITIES]; // Changable by the user
    bool is_active_in_hierarchy[MAX_ENTITIES];
    
    // The tightly packed component arrays
    NameComponent names[MAX_ENTITIES];
    Transform transforms[MAX_ENTITIES];
    RenderComponent renderables[MAX_ENTITIES];
    CameraComponent cameras[MAX_ENTITIES];
    PointLightComponent point_lights[MAX_ENTITIES];
    ColliderComponent colliders[MAX_ENTITIES];
    RigidbodyComponent rigidbodies[MAX_ENTITIES];
    ScriptComponent scripts[MAX_ENTITIES];

    uint32_t main_camera_id;

    DirectionalLight global_light;

    PhysicsWorldHandle physics_world;
} Scene;





// --- Scene API ---
void Scene_Init(Scene* scene);
void Scene_Update(Scene* scene);
void Scene_UpdateTransforms(Scene* scene);
Entity Scene_GetEntity(Scene* scene, const char* name);
void Scene_SetMainCamera(Scene* scene, Entity camera_entity);
void Scene_ShutdownPhysics(Scene* scene);



// --- Entity Lifecycle API ---
Entity Entity_Create(Scene* scene, const char* name);
void Entity_Destroy(Entity entity);
bool Entity_IsValid(Entity entity);
void Entity_SetParent(Entity child, Entity parent);
void Entity_RemoveParent(Entity child);
void Entity_SetActive(Entity entity, bool active);
void Entity_RemovePhysics(Entity entity);
void Entity_RemoveRigidbody(Entity entity);



// Removing components
void Entity_RemoveComponent(Entity entity, ComponentMask component);
void Entity_UnbindScript(Entity entity, void* target_instance_data);



// Component Setters
void Entity_SetName(Entity entity, const char* name);
void Entity_AddTransform(Entity entity, Vector3 position, Quaternion rotation, Vector3 scale);
void Entity_AddRenderable(Entity entity, uint32_t mesh_id, uint32_t material_id);
void Entity_AddCamera(Entity entity, float fov, float nearZ, float farZ);
void Entity_AddPointLight(Entity entity, Vector3 color, float intensity, float constant, float linear, float quadratic);
void Entity_AddColliderBox(Entity entity, Vector3 extents, bool is_trigger);
void Entity_AddColliderBoxAuto(Entity entity, bool is_trigger);
void Entity_AddColliderSphere(Entity entity, float radius, bool is_trigger);
void Entity_AddColliderMesh(Entity entity, MeshHandle mesh, bool is_trigger);
void Entity_AddRigidbody(Entity entity, float mass);
void Entity_BindScript(Entity entity, void* data, ScriptStartFunc start, ScriptUpdateFunc update, ScriptDestroyFunc destroy);



// Component Getters
Transform* Entity_GetTransform(Entity entity);
RenderComponent* Entity_GetRenderable(Entity entity);
MeshHandle Entity_GetMesh(Entity entity);
CameraComponent* Entity_GetCamera(Entity entity);
PointLightComponent* Entity_GetPointLight(Entity entity);
ColliderComponent* Entity_GetCollider(Entity entity);
RigidbodyComponent* Entity_GetRigidbody(Entity entity);



// Transform setters and getters
void Transform_SetLocalPosition(Transform* t, Vector3 position);
void Transform_SetLocalRotationEuler(Transform* t, Vector3 euler_angles);
void Transform_SetLocalRotation(Transform* t, Quaternion rotation);
void Transform_SetLocalScale(Transform* t, Vector3 scale);

void Transform_Translate(Transform* t, Vector3 translation);
void Transform_RotateEuler(Transform* t, Vector3 euler_addition);

Vector3 Transform_GetLocalPosition(Transform* t);
Vector3 Transform_GetGlobalPosition(Transform* t);



// Rigidbody setters
void Rigidbody_SetGravity(Entity entity, bool use_gravity);
void Rigidbody_SetKinematic(Entity entity, bool is_kinematic);



#endif