#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../core/math_core.h"
#include "../core/time_core.h"
#include "../core/log.h"
#include "../audio/audio.h"
#include "../assets/asset_manager.h"
#include "physics_bridge.h"
#include "scene_structures.h"





// --- Scene API ---

Scene* Scene_Create();
void Scene_Destroy(Scene* scene);
void Scene_Init(Scene* scene);
void Scene_Clear(Scene* scene);
bool Scene_Save(Scene* scene, const char* filepath);
bool Scene_Load(Scene* scene, const char* filepath);
void Scene_Update(Scene* scene);
void Scene_FixedUpdate(Scene* scene);
void Scene_UpdateScripts(Scene* scene);
void Scene_FixedUpdateScripts(Scene* scene);
void Scene_UpdateAudio(Scene* scene);
void Scene_UpdateLineRenderers(Scene* scene);
void Scene_UpdateAnimators(Scene* scene, float delta_time);
void Scene_UpdateBoneAttachments(Scene* scene);
void Scene_UpdateTransforms(Scene* scene);
void Scene_SyncPhysicsPreSim(Scene* scene);
void Scene_StepPhysicsAndCollisions(Scene* scene);
void Scene_SyncPhysicsPostSim(Scene* scene);
Entity Scene_GetEntity(Scene* scene, const char* name);
uint32_t Scene_GetTotalEntityCount(Scene* scene);
uint32_t Scene_GetActiveEntityCount(Scene* scene);
uint32_t Scene_GetEntitiesWithTag(Scene* scene, const char* target_tag, Entity* out_array, uint32_t max_results);
void Scene_SetMainCamera(Scene* scene, Entity camera_entity);
void Scene_ShutdownPhysics(Scene* scene);
void Scene_ProcessDestroyQueue(Scene* scene);
void Scene_SetSkybox(Scene* scene, Texture* skybox_texture, Shader* skybox_shader);
void Scene_RemoveSkybox(Scene* scene);
bool Scene_Raycast(Scene* scene, Ray ray, float max_distance, RaycastHit* out_hit, int collision_mask, bool hit_triggers);
int Scene_RaycastAll(Scene* scene, Ray ray, float max_distance, RaycastHit* out_hits, int max_hits, int collision_mask, bool hit_triggers);



// --- Entity Lifecycle API ---

Entity Entity_Create(Scene* scene, const char* name);
void Entity_Destroy(Entity entity);
bool Entity_IsValid(Entity entity);
void Entity_SetParent(Entity child, Entity parent);
Entity Entity_GetParent(Entity entity);
void Entity_RemoveParent(Entity child);
void Entity_SetActive(Entity entity, bool active);
void Entity_AddModel(Entity parent, Model* model);
uint32_t Entity_GetChildren(Entity entity, uint32_t* out_array, uint32_t max_count, bool recursive);
Entity Entity_GetParentWithComponent(Entity entity, uint32_t component_mask);
Entity Entity_FindChildByName(Entity entity, const char* name);
int Entity_GetAnimatorBoneIndex(Entity entity, const char* bone_name);
void Entity_RemovePhysics(Entity entity);
void Entity_RemoveRigidbody(Entity entity);



// --- Removing components ---

void Entity_RemoveComponent(Entity entity, ComponentMask component);
void Entity_UnbindScript(Entity entity, void* target_instance_data);



// --- Component Setters ---

void Entity_SetName(Entity entity, const char* name);
void Entity_SetTag(Entity entity, const char* name);
void Entity_AddTransform(Entity entity, Vector3 position, Quaternion rotation, Vector3 scale);
void Entity_AddMeshRenderer(Entity entity, Mesh* mesh, Material* material);
void Entity_AddSkinnedMeshRenderer(Entity entity, SkinnedMesh* mesh, Material* material, uint32_t animator_id);
void Entity_AddCamera(Entity entity, float fov, float nearZ, float farZ);
void Entity_AddLight(Entity entity, LightType type, Color color);
void Entity_AddColliderBox(Entity entity, Vector3 extents, bool is_trigger);
void Entity_AddColliderBoxAuto(Entity entity, bool is_trigger);
void Entity_AddColliderSphere(Entity entity, float radius, bool is_trigger);
void Entity_AddColliderMesh(Entity entity, Mesh* mesh, bool is_trigger, bool is_convex);
void Entity_AddRigidbody(Entity entity, float mass);
void Entity_AddAudioListener(Entity entity);
void Entity_AddAudioSource(Entity entity);
void Entity_AddAnimator(Entity entity, void* raw_skeleton, void* raw_clip);
void Entity_AddBoneAttachment(Entity entity, int bone_index, Matrix4 offset);
void Entity_AddLineRenderer(Entity entity, Material* material);
void Entity_AddSpriteRenderer(Entity entity, Material* material);
void Entity_BindScript(Entity entity, ScriptInstance new_script);
void Script_SetActive(Entity entity, void* instance_data, bool active);
void Bridge_SpawnScript(Entity raw_e, const char* class_name, struct cJSON* json_data);



// --- Component Getters ---

const char* Entity_GetName(Entity entity);
const char* Entity_GetTag(Entity entity);
Transform* Entity_GetTransform(Entity entity);
MeshRendererComponent* Entity_GetMeshRenderer(Entity entity);
SkinnedMeshRendererComponent* Entity_GetSkinnedMeshRenderer(Entity entity);
Mesh* Entity_GetMesh(Entity entity);
CameraComponent* Entity_GetCamera(Entity entity);
LightComponent* Entity_GetLight(Entity entity);
ColliderComponent* Entity_GetCollider(Entity entity);
RigidbodyComponent* Entity_GetRigidbody(Entity entity);
AudioListenerComponent* Entity_GetAudioListener(Entity entity);
AudioSourceComponent* Entity_GetAudioSource(Entity entity);
AnimatorComponent* Entity_GetAnimator(Entity entity);
BoneAttachmentComponent* Entity_GetBoneAttachment(Entity entity);
LineRendererComponent* Entity_GetLineRenderer(Entity entity);
SpriteRendererComponent* Entity_GetSpriteRenderer(Entity entity);
ScriptComponent* Entity_GetScripts(Entity entity);



// --- Transform setters and getters ---

void Transform_SetLocalPosition(Transform* t, Vector3 position);
void Transform_SetLocalRotationEuler(Transform* t, Vector3 euler_angles);
void Transform_SetLocalRotation(Transform* t, Quaternion rotation);
void Transform_SetLocalScale(Transform* t, Vector3 scale);

void Transform_SetGlobalPosition(Transform* t, Transform* parent_t, Vector3 global_position);
void Transform_SetGlobalRotation(Transform* t, Transform* parent_t, Quaternion global_rotation);
void Transform_SetGlobalRotationEuler(Transform* t, Transform* parent_t, Vector3 global_euler);
void Transform_SetGlobalScale(Transform* t, Transform* parent_t, Vector3 global_scale);

void Transform_Translate(Transform* t, Vector3 translation);
void Transform_RotateEuler(Transform* t, Vector3 euler_addition);

Vector3 Transform_GetLocalPosition(Transform* t);
Vector3 Transform_GetGlobalPosition(Transform* t);
Vector3 Transform_GetGlobalScale(Transform* t);
Quaternion Transform_GetGlobalRotation(Transform* t);

Vector3 Transform_GetForwardVector(Transform* t);
Vector3 Transform_GetRightVector(Transform* t);
Vector3 Transform_GetUpVector(Transform* t);



// --- Rigidbody setters and functions ---

void Rigidbody_SetGravity(Entity entity, bool use_gravity);
void Rigidbody_SetKinematic(Entity entity, bool is_kinematic);
void Rigidbody_SetLinearVelocity(Entity entity, Vector3 velocity);
void Rigidbody_MovePosition(Entity entity, Vector3 position);



// --- Collider setters ---

void Collider_SetLayerAndMask(Entity entity, CollisionLayer layer, int mask);
void Collider_SetBoxExtents(Entity entity, Vector3 new_extents);
void Collider_SetSphereRadius(Entity entity, float new_radius);
void Collider_SetMeshScale(Entity entity, Vector3 scale);
void Collider_SetConvex(Entity entity, bool is_convex);



// --- Camera setters and functions ---
void Camera_RecalculateProjectionIfNeeded(CameraComponent* cam);
Ray Camera_ScreenPointToRay(CameraComponent* cam, Transform* cam_transform, float mouse_x, float mouse_y);
Vector2 Camera_WorldToScreenPoint(CameraComponent* cam, Transform* cam_transform, Vector3 world_pos);
void Camera_SetFOV(CameraComponent* cam, float FOV);
// TODO: Implement camera culling masks



// --- Renderable setters ---

void MeshRenderer_SetMaterial(MeshRendererComponent* r, Material* material);



// --- Line Renderer Function ---

void LineRenderer_AddPoint(LineRendererComponent* line, Vector3 point);
void LineRenderer_ClearPoints(LineRendererComponent* line);
void LineRenderer_SetPoint(LineRendererComponent* line, uint32_t index, Vector3 point);
Vector3 LineRenderer_GetPoint(LineRendererComponent* line, uint32_t index);
void LineRenderer_SetPoints(LineRendererComponent* line, Vector3* points, uint32_t count);




#endif