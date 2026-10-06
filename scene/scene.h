#ifndef SCENE_H
#define SCENE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../core/math_core.h"
#include "../core/time_core.h"
#include "../core/log_core.h"
#include "../audio/audio.h"
#include "../assets/asset_manager.h"
#include "physics_bridge.h"
#include "../core/frustum_core.h"
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
void Scene_UpdateSkinnedMeshBounds(Scene* scene);
void Scene_UpdateBoneAttachments(Scene* scene);
void Scene_UpdateTransforms(Scene* scene);

void Scene_SyncPhysicsPreSim(Scene* scene);
void Scene_StepPhysicsAndCollisions(Scene* scene);
void Scene_SyncPhysicsPostSim(Scene* scene);

Entity Scene_GetEntity(Scene* scene, const char* name);
uint32_t Scene_GetTotalEntityCount(Scene* scene);
uint32_t Scene_GetActiveEntityCount(Scene* scene);
uint32_t Scene_GetAllEntities(Scene* scene, Entity* out_array, uint32_t max_results);
uint32_t Scene_GetEntitiesWithTag(Scene* scene, const char* target_tag, Entity* out_array, uint32_t max_results);

void Scene_SetMainCamera(Scene* scene, Entity camera_entity);
void Scene_SetGravity(Scene* scene, Vector3 gravity);
Vector3 Scene_GetGravity(Scene* scene);
void Scene_ShutdownPhysics(Scene* scene);
void Scene_ProcessDestroyQueue(Scene* scene);
void Scene_SetEnvironmentMap(Scene* scene, EnvironmentMap* env_map);
void Scene_RemoveEnvironmentMap(Scene* scene);
void Scene_SetExposure(Scene* scene, float exposure);
float Scene_GetExposure(Scene* scene);
bool Scene_Raycast(Scene* scene, Ray ray, float max_distance, RaycastHit* out_hit, CollisionMask collision_mask, bool hit_triggers);
int Scene_RaycastAll(Scene* scene, Ray ray, float max_distance, RaycastHit* out_hits, int max_hits, CollisionMask collision_mask, bool hit_triggers);



// --- Retained UI API ---

void RetainedUI_Reset(Scene* scene);
void RetainedUI_Shutdown(Scene* scene);
void RetainedUI_PreUpdate(Scene* scene, uint32_t window_w, uint32_t window_h, float mouse_x, float mouse_y, bool mouse_captured);

uint32_t RetainedUI_GatherCanvases(Scene* scene);
void RetainedUI_UpdateLayout(Scene* scene, uint32_t window_w, uint32_t window_h);
void RetainedUI_ProcessPointer(Scene* scene, float mouse_x, float mouse_y, bool mouse_captured);
void RetainedUI_BuildOverlay(Scene* scene);










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
void Entity_AddTransform(Entity entity);
void Entity_AddMeshRenderer(Entity entity);
void Entity_AddSkinnedMeshRenderer(Entity entity);
void Entity_AddCamera(Entity entity);
void Entity_AddLight(Entity entity);
void Entity_AddColliderBox(Entity entity);
void Entity_AddColliderBoxAuto(Entity entity);
void Entity_AddColliderSphere(Entity entity);
void Entity_AddColliderMesh(Entity entity);
void Entity_AddRigidbody(Entity entity);
void Entity_AddAudioListener(Entity entity);
void Entity_AddAudioSource(Entity entity);
void Entity_AddAnimator(Entity entity);
void Entity_AddBoneAttachment(Entity entity);
void Entity_AddLineRenderer(Entity entity);
void Entity_AddSpriteRenderer(Entity entity);
void Entity_AddReflectionProbe(Entity entity);
void Entity_AddUICanvas(Entity entity);
void Entity_AddRectTransform(Entity entity);
void Entity_AddUIImage(Entity entity);
void Entity_AddUIText(Entity entity);
void Entity_AddUIButton(Entity entity);
void Entity_BindScript(Entity entity, ScriptInstance new_script);
void Script_SetActive(Entity entity, void* instance_data, bool active);
void Bridge_SpawnScript(Entity raw_e, const char* class_name, struct cJSON* json_data);
bool Script_IsActive(Entity entity, void* instance_data);



// --- Component Getters ---

const char* Entity_GetName(Entity entity);
const char* Entity_GetTag(Entity entity);
Transform* Entity_GetTransform(Entity entity);
MeshRendererComponent* Entity_GetMeshRenderer(Entity entity);
SkinnedMeshRendererComponent* Entity_GetSkinnedMeshRenderer(Entity entity);
Mesh* Entity_GetMesh(Entity entity);
SkinnedMesh* Entity_GetSkinnedMesh(Entity entity);
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
ReflectionProbeComponent* Entity_GetReflectionProbe(Entity entity);
UICanvasComponent* Entity_GetUICanvas(Entity entity);
RectTransformComponent* Entity_GetRectTransform(Entity entity);
UIImageComponent* Entity_GetUIImage(Entity entity);
UITextComponent* Entity_GetUIText(Entity entity);
UIButtonComponent* Entity_GetUIButton(Entity entity);
ScriptComponent* Entity_GetScripts(Entity entity);










// --- Transform Setters and Getters ---

void Transform_SetLocalPosition(Transform* t, Vector3 position);
void Transform_SetLocalRotation(Transform* t, Quaternion rotation);
void Transform_SetLocalRotationEuler(Transform* t, Vector3 euler_angles);
void Transform_SetLocalScale(Transform* t, Vector3 scale);

void Transform_SetGlobalPosition(Transform* t, Transform* parent_t, Vector3 global_position);
void Transform_SetGlobalRotation(Transform* t, Transform* parent_t, Quaternion global_rotation);
void Transform_SetGlobalRotationEuler(Transform* t, Transform* parent_t, Vector3 global_euler);
void Transform_SetGlobalScale(Transform* t, Transform* parent_t, Vector3 global_scale);

Vector3 Transform_GetLocalPosition(Transform* t);
Vector3 Transform_GetGlobalPosition(Transform* t);
Quaternion Transform_GetGlobalRotation(Transform* t);
Vector3 Transform_GetGlobalScale(Transform* t);

Vector3 Transform_GetForwardVector(Transform* t);
Vector3 Transform_GetRightVector(Transform* t);
Vector3 Transform_GetUpVector(Transform* t);

void Transform_Translate(Transform* t, Vector3 translation);
void Transform_RotateEuler(Transform* t, Vector3 euler_addition);



// --- Rigidbody setters and functions ---

void Rigidbody_SetGravity(Entity entity, bool use_gravity);
void Rigidbody_SetKinematic(Entity entity, bool is_kinematic);
void Rigidbody_SetLinearVelocity(Entity entity, Vector3 velocity);
void Rigidbody_MovePosition(Entity entity, Vector3 position);
void Rigidbody_AddForce(Entity entity, Vector3 force, ForceMode mode);
void Rigidbody_AddForceAtPosition(Entity entity, Vector3 force, Vector3 world_point, ForceMode mode);
void Rigidbody_SetMass(Entity entity, float mass);
void Rigidbody_SetDamping(Entity entity, float linear_drag, float angular_drag);
void Rigidbody_SetRotationConstraints(Entity entity, bool freeze_x, bool freeze_y, bool freeze_z);



// --- Collider setters ---

void Collider_SetLayerAndMask(Entity entity, CollisionLayer layer, CollisionMask mask);
void Collider_SetBoxExtents(Entity entity, Vector3 new_extents);
void Collider_SetSphereRadius(Entity entity, float new_radius);
void Collider_SetMeshScale(Entity entity, Vector3 scale);
void Collider_SetConvex(Entity entity, bool is_convex);
void Collider_SetMesh(Entity entity, Mesh* mesh, bool is_convex);
void Collider_SetTrigger(Entity entity, bool is_trigger);



// --- Camera setters and functions ---
void Camera_RecalculateProjectionIfNeeded(CameraComponent* cam);
Ray Camera_ScreenPointToRay(CameraComponent* cam, Transform* cam_transform, float mouse_x, float mouse_y);
Vector2 Camera_WorldToScreenPoint(CameraComponent* cam, Transform* cam_transform, Vector3 world_pos);
void Camera_SetFOV(CameraComponent* cam, float FOV);
// TODO: Implement camera culling masks



// --- Mesh Renderable setters ---

void MeshRenderer_SetMesh(MeshRendererComponent* r, Mesh* mesh);
void MeshRenderer_SetMaterial(MeshRendererComponent* r, Material* material);



// --- Skinned Mesh Renderable setters ---

void SkinnedMeshRenderer_SetMesh(SkinnedMeshRendererComponent* r, SkinnedMesh* mesh);
void SkinnedMeshRenderer_SetMaterial(SkinnedMeshRendererComponent* r, Material* material);
void SkinnedMeshRenderer_SetRootAnimator(SkinnedMeshRendererComponent* r, Entity root);



// --- Animator setters ---

void Animator_SetSkeleton(Entity entity, Skeleton* skeleton);
void Animator_SetClip(Entity entity, AnimationClip* clip);



// --- Line Renderer Function ---

void LineRenderer_AddPoint(LineRendererComponent* line, Vector3 point);
void LineRenderer_ClearPoints(LineRendererComponent* line);
void LineRenderer_SetPoint(LineRendererComponent* line, uint32_t index, Vector3 point);
Vector3 LineRenderer_GetPoint(LineRendererComponent* line, uint32_t index);
void LineRenderer_SetPoints(LineRendererComponent* line, Vector3* points, uint32_t count);



// --- Sprite Renderer Function ---

void SpriteRenderer_SetSprite(SpriteRendererComponent* comp, Texture* sprite);



// --- Reflection Probe Functions ---

void ReflectionProbe_SetBoxExtents(ReflectionProbeComponent* probe, Vector3 extents);
void ReflectionProbe_SetBlendDistance(ReflectionProbeComponent* probe, float blend_distance);
void ReflectionProbe_SetPriority(ReflectionProbeComponent* probe, int32_t priority);
void ReflectionProbe_SetCaptureResolution(ReflectionProbeComponent* probe, uint32_t resolution);
void ReflectionProbe_MarkDirty(ReflectionProbeComponent* probe);



// --- UI Canvas Functions ---

void UICanvas_SetActive(Entity entity, bool active);
void UICanvas_SetScaleMode(Entity entity, UICanvasScaleMode mode);
void UICanvas_SetReferenceResolution(Entity entity, Vector2 resolution);
void UICanvas_SetMatchWidthOrHeight(Entity entity, float match);



// --- UI Rect Transform Functions ---

void RectTransform_MarkDirty(Entity entity);
void RectTransform_SetAnchoredPosition(Entity entity, Vector2 position);
void RectTransform_SetSizeDelta(Entity entity, Vector2 size);
void RectTransform_SetAnchors(Entity entity, Vector2 min, Vector2 max);
void RectTransform_SetPivot(Entity entity, Vector2 pivot);



// --- UI Text Transform Functions ---

void UIText_SetText(Entity entity, const char* text);
void UIText_SetFont(Entity entity, Font* font);





#endif