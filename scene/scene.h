#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "../core/mathCore.h"
#include "../core/timeCore.h"
#include "../core/log.h"
#include "../assets/asset_manager.h"

#define MAX_ENTITIES 4096
#define MAX_NAME_LENGTH 64
#define MAX_SCRIPTS_PER_ENTITY 64


typedef struct Scene Scene;

// --- The Entity Handle ---
// Passed by value because it is incredibly lightweight.
typedef struct Entity
{
    uint32_t id;
    Scene* scene;
} Entity;





// --- Component Masks ---
typedef enum
{
    COMPONENT_NONE        = 0,
    COMPONENT_NAME        = 1 << 0,
    COMPONENT_TRANSFORM   = 1 << 1,
    COMPONENT_RENDER      = 1 << 2,
    COMPONENT_CAMERA      = 1 << 3,
    COMPONENT_POINT_LIGHT = 1 << 4,
    COMPONENT_SCRIPT      = 1 << 5
} ComponentMask;



// Function pointers for custom scripts
typedef void (*ScriptUpdateFunc)(Entity entity, float dt, void* instance_data);
typedef void (*ScriptStartFunc)(Entity entity, void* instance_data);
typedef void (*ScriptDestroyFunc)(Entity entity, void* instance_data);


// -------------------------------- //
// --- All Component Structures --- //
// -------------------------------- //


typedef struct NameComponent
{
    char name[MAX_NAME_LENGTH];
} NameComponent;

// Transform: an entities positional parts
typedef struct Transform
{
    Vector3 position;
    Vector3 rotation_euler;
    Quaternion rotation;
    Vector3 scale;
    Matrix4 world_matrix; 
    bool is_dirty; 
} Transform;


// Component that holds rendering information
typedef struct RenderComponent
{
    uint32_t mesh_id;
    uint32_t shader_id;
    uint32_t texture_id;
} RenderComponent;


// --- The Camera Component ---
typedef struct CameraComponent
{
    float fov;
    float nearZ;
    float farZ;
    Matrix4 projection_matrix; // We can cache the projection matrix here so we don't recalculate it every frame!
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


// Instance of a script
typedef struct ScriptInstance
{
    void* instance_data;
    ScriptStartFunc OnStart;
    ScriptUpdateFunc OnUpdate;
    ScriptDestroyFunc OnDestroy;
    bool has_started;
} ScriptInstance;


// Custom scripting component
typedef struct ScriptComponent
{
    ScriptInstance instances[MAX_SCRIPTS_PER_ENTITY];
    uint32_t count;
} ScriptComponent;


// --- The Scene (The Data Container) ---
typedef struct Scene
{
    uint32_t component_masks[MAX_ENTITIES];
    
    // The tightly packed component arrays
    NameComponent names[MAX_ENTITIES];
    Transform transforms[MAX_ENTITIES];
    RenderComponent renderables[MAX_ENTITIES];
    CameraComponent cameras[MAX_ENTITIES];
    PointLightComponent point_lights[MAX_ENTITIES];
    ScriptComponent scripts[MAX_ENTITIES];

    uint32_t main_camera_id;

    DirectionalLight global_light;
} Scene;





// --- Scene API ---
void Scene_Init(Scene* scene);
void Scene_Update(Scene* scene);

// --- Entity Lifecycle API ---
Entity Entity_Create(Scene* scene, const char* name);
void Entity_Destroy(Entity entity);
bool Entity_IsValid(Entity entity);

// --- Component Setters ---
void Entity_SetName(Entity entity, const char* name);
void Entity_AddTransform(Entity entity, Vector3 position, Quaternion rotation, Vector3 scale);
void Entity_SetPosition(Entity entity, Vector3 position);
void Entity_SetRotationEuler(Entity entity, Vector3 euler_angles);
void Entity_SetScale(Entity entity, Vector3 scale);
void Entity_AddRenderable(Entity entity, uint32_t mesh_id, uint32_t shader_id, uint32_t texture_id);
void Entity_AddCamera(Entity entity, float fov, float nearZ, float farZ);
void Entity_AddPointLight(Entity entity, Vector3 color, float intensity, float constant, float linear, float quadratic);
void Entity_BindScript(Entity entity, void* data, ScriptStartFunc start, ScriptUpdateFunc update, ScriptDestroyFunc destroy);

void Scene_SetMainCamera(Scene* scene, Entity camera_entity);

// --- Component Getters (Returns a pointer to the actual data so the user can modify it!) ---
Entity Scene_GetEntity(Scene* scene, const char* name);
Transform* Entity_GetTransform(Entity entity);
RenderComponent* Entity_GetRenderable(Entity entity);
CameraComponent* Entity_GetCamera(Entity entity);

// Removing components
void Entity_RemoveComponent(Entity entity, ComponentMask component);
void Entity_UnbindScript(Entity entity, void* target_instance_data);

// Other functions
void Entity_Translate(Entity entity, Vector3 translation);
void Entity_RotateEuler(Entity entity, Vector3 euler_addition);

#endif