#include "../../include/scene/Components.hpp"
#include "../../include/scene/Scene.hpp"


extern "C"
{
    #include "../../../scene/scene.h"
    #include <cstring>
    #include <cstddef>
}


static_assert(sizeof(Prism::UICanvasComponent) == sizeof(::UICanvasComponent), "UICanvasComponent bridge layout mismatch");
static_assert(sizeof(Prism::RectTransformComponent) == sizeof(::RectTransformComponent), "RectTransformComponent bridge layout mismatch");
static_assert(sizeof(Prism::UIImageComponent) == sizeof(::UIImageComponent), "UIImageComponent bridge layout mismatch");
static_assert(sizeof(Prism::UITextComponent) == sizeof(::UITextComponent), "UITextComponent bridge layout mismatch");
static_assert(sizeof(Prism::UIButtonComponent) == sizeof(::UIButtonComponent), "UIButtonComponent bridge layout mismatch");
static_assert(offsetof(Prism::UICanvasComponent, sort_order) == offsetof(::UICanvasComponent, sort_order), "UICanvasComponent field offset mismatch");
static_assert(offsetof(Prism::RectTransformComponent, anchored_position) == offsetof(::RectTransformComponent, anchored_position), "RectTransformComponent field offset mismatch");
static_assert(offsetof(Prism::UIImageComponent, color) == offsetof(::UIImageComponent, color), "UIImageComponent field offset mismatch");
static_assert(offsetof(Prism::UITextComponent, font_size) == offsetof(::UITextComponent, font_size), "UITextComponent field offset mismatch");
static_assert(offsetof(Prism::UIButtonComponent, clicked_this_frame) == offsetof(::UIButtonComponent, clicked_this_frame), "UIButtonComponent field offset mismatch");


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
    
    void Transform::SetGlobalPosition(const Prism::Vector3& pos) {
        ::Transform* parent_t = nullptr;
        if (this->parent_id != 0xFFFFFFFF)
            parent_t = &static_cast<::Scene*>(this->entity.scene_ptr)->transforms[this->parent_id];
        ::Transform_SetGlobalPosition(reinterpret_cast<::Transform*>(this), parent_t, {pos.x, pos.y, pos.z});
    }
    void Transform::SetGlobalRotationEuler(const Prism::Vector3& euler) {
        ::Transform* parent_t = nullptr;
        if (this->parent_id != 0xFFFFFFFF)
            parent_t = &static_cast<::Scene*>(this->entity.scene_ptr)->transforms[this->parent_id];
        ::Transform_SetGlobalRotationEuler(reinterpret_cast<::Transform*>(this), parent_t, {euler.x, euler.y, euler.z});
    }
    void Transform::SetGlobalRotation(const Prism::Quaternion& rot) {
        ::Transform* parent_t = nullptr;
        if (this->parent_id != 0xFFFFFFFF)
            parent_t = &static_cast<::Scene*>(this->entity.scene_ptr)->transforms[this->parent_id];
        ::Transform_SetGlobalRotation(reinterpret_cast<::Transform*>(this), parent_t, {rot.x, rot.y, rot.z, rot.w});
    }
    void Transform::SetGlobalScale(const Prism::Vector3& scale) {
        ::Transform* parent_t = nullptr;
        if (this->parent_id != 0xFFFFFFFF)
            parent_t = &static_cast<::Scene*>(this->entity.scene_ptr)->transforms[this->parent_id];
        ::Transform_SetGlobalScale(reinterpret_cast<::Transform*>(this), parent_t, {scale.x, scale.y, scale.z});
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
    // Light Component Implementation
    // ==========================================

    void LightComponent::SetCascadedShadows(uint8_t cascade_count, float max_distance, float split_lambda, float blend_fraction) {
        if (cascade_count < 2)
            cascade_count = 2;
        if (cascade_count > 4)
            cascade_count = 4;
        this->shadow_cascade_count = cascade_count;
        this->shadow_max_distance = max_distance;
        this->cascade_split_lambda = split_lambda;
        this->cascade_blend_fraction = blend_fraction;
    }

    void LightComponent::DisableCascadedShadows() {
        this->shadow_cascade_count = 1;
    }



    // ==========================================
    // Mesh Renderer Component Implementation
    // ==========================================

    void MeshRendererComponent::SetMesh(Prism::Mesh mesh) {
        ::MeshRenderer_SetMesh(reinterpret_cast<::MeshRendererComponent*>(this), static_cast<::Mesh*>(mesh.GetRaw()));
    }
    void MeshRendererComponent::SetMaterial(Prism::Material material) {
        ::MeshRenderer_SetMaterial(reinterpret_cast<::MeshRendererComponent*>(this), static_cast<::Material*>(material.GetRaw()));
    }
    void MeshRendererComponent::SetLayerMask(uint8_t layer_index) {
        this->layer_mask = (1u << layer_index); // Sets the object to a specific layer (0 through 31)
    }



    // ==========================================
    // Skinned Mesh Renderer Component Implementation
    // ==========================================

    void SkinnedMeshRendererComponent::SetMesh(Prism::SkinnedMesh mesh) {
        ::SkinnedMeshRenderer_SetMesh(reinterpret_cast<::SkinnedMeshRendererComponent*>(this), static_cast<::SkinnedMesh*>(mesh.GetRaw()));
    }
    void SkinnedMeshRendererComponent::SetMaterial(Prism::Material material) {
        ::SkinnedMeshRenderer_SetMaterial(reinterpret_cast<::SkinnedMeshRendererComponent*>(this), static_cast<::Material*>(material.GetRaw()));
    }
    void SkinnedMeshRendererComponent::SetLayerMask(uint8_t layer_index) {
        this->layer_mask = (1u << layer_index); // Sets the object to a specific layer (0 through 31)
    }
    void SkinnedMeshRendererComponent::SetRootAnimator(Prism::Entity entity) {
        ::Entity raw_e = { entity.id, static_cast<::Scene*>(entity.scene_ptr) };
        ::SkinnedMeshRenderer_SetRootAnimator(reinterpret_cast<::SkinnedMeshRendererComponent*>(this), raw_e);
    }



    // ==========================================
    // Camera Component Implementation
    // ==========================================

    Prism::Ray CameraComponent::ScreenPointToRay(const Prism::Vector2& screenPoint) const {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::CameraComponent* cam = &static_cast<::Scene*>(e.scene)->cameras[e.id];
        ::Transform* t = &static_cast<::Scene*>(e.scene)->transforms[e.id];

        ::Ray c_ray = ::Camera_ScreenPointToRay(cam, t, screenPoint.x, screenPoint.y);
        
        return Prism::Ray{ Prism::Vector3(c_ray.origin.x, c_ray.origin.y, c_ray.origin.z), Prism::Vector3(c_ray.direction.x, c_ray.direction.y, c_ray.direction.z) };
    }
    Prism::Vector2 CameraComponent::WorldToScreenPoint(const Prism::Vector3& worldPosition) const {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::CameraComponent* cam = &static_cast<::Scene*>(e.scene)->cameras[e.id];
        ::Transform* t = &static_cast<::Scene*>(e.scene)->transforms[e.id];

        ::Vector3 pos = {worldPosition.x, worldPosition.y, worldPosition.z};
        ::Vector2 screen_pos = ::Camera_WorldToScreenPoint(cam, t, pos);
        
        return Prism::Vector2(screen_pos.x, screen_pos.y);
    }
    void CameraComponent::SetCullingMask(uint32_t mask) {
        this->culling_masks = mask; // Shift a '1' over by 'layer_index' spaces
    }
    void CameraComponent::AddLayerToMask(uint8_t layer_index) {
        this->culling_masks |= (1u << layer_index); // Add a specific layer to the camera's sight
    }
    void CameraComponent::RemoveLayerFromMask(uint8_t layer_index) {
        this->culling_masks &= ~(1u << layer_index); // Remove a specific layer from the camera's sight
    }
    void CameraComponent::SetViewportPosition(uint32_t x, uint32_t y) {
        this->viewport_x = x;
        this->viewport_y = y;
    }
    void CameraComponent::SetViewportSize(uint32_t width, uint32_t height) {
        this->viewport_width = width;
        this->viewport_height = height;
    }
    void CameraComponent::SetFOV(float fov) {
        this->fov = fov;
    }



    // ==========================================
    // Rigidbody Implementation
    // ==========================================
    
    void RigidbodyComponent::SetMass(float mass) { 
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetMass(raw_e, mass); 
    }

    void RigidbodyComponent::SetLinearDrag(float linear_drag) { 
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetDamping(raw_e, linear_drag, this->angular_drag); 
    }

    void RigidbodyComponent::SetAngularDrag(float angular_drag) { 
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetDamping(raw_e, this->linear_drag, angular_drag); 
    }

    void RigidbodyComponent::SetGravity(bool use_gravity) { 
        // Reconstruct the ::Entity from Prism::Entity
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetGravity(raw_e, use_gravity); 
    }
    
    void RigidbodyComponent::SetKinematic(bool kinematic) { 
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetKinematic(raw_e, kinematic); 
    }

    void RigidbodyComponent::SetFreezeRotation(bool freeze_x, bool freeze_y, bool freeze_z) { 
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetRotationConstraints(raw_e, freeze_x, freeze_y, freeze_z); 
    }



    void RigidbodyComponent::SetLinearVelocity(Prism::Vector3& velocity) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_SetLinearVelocity(raw_e, ::Vector3{velocity.x, velocity.y, velocity.z}); 
    }

    void RigidbodyComponent::MovePosition(const Prism::Vector3& position) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_MovePosition(raw_e, ::Vector3{position.x, position.y, position.z});
    }

    void RigidbodyComponent::AddForce(const Prism::Vector3& force, ForceMode mode) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_AddForce(raw_e, ::Vector3{force.x, force.y, force.z}, static_cast<::ForceMode>(mode));
    }

    void RigidbodyComponent::AddForceAtPosition(const Prism::Vector3& force, const Prism::Vector3& worldPoint, ForceMode mode) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Rigidbody_AddForceAtPosition(raw_e, ::Vector3{force.x, force.y, force.z}, ::Vector3{worldPoint.x, worldPoint.y, worldPoint.z}, static_cast<::ForceMode>(mode));
    }



    // ==========================================
    // Collider Implementation
    // ==========================================
    
    void ColliderComponent::SetLayerAndMask(CollisionLayer layer, CollisionMask mask) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetLayerAndMask(raw_e, static_cast<::CollisionLayer>(layer), static_cast<::CollisionMask>(mask));
    }

    void ColliderComponent::SetTrigger(bool is_trigger) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetTrigger(raw_e, is_trigger);
    }

    void BoxColliderComponent::SetBoxExtents(const Prism::Vector3& new_extents) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetBoxExtents(raw_e, {new_extents.x, new_extents.y, new_extents.z});
    }

    void SphereColliderComponent::SetSphereRadius(float new_radius) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetSphereRadius(raw_e, new_radius);
    }

    void MeshColliderComponent::SetMeshScale(const Prism::Vector3& new_scale) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetMeshScale(raw_e, {new_scale.x, new_scale.y, new_scale.z});
    }

    void MeshColliderComponent::SetMesh(Prism::Mesh mesh, bool is_convex) {
        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetMesh(raw_e, static_cast<::Mesh*>(mesh.GetRaw()), is_convex);
    }

    void ColliderComponent::SetConvex(bool is_convex) {
        if (type != COLLIDER_MESH) {
            Debug_Warning("Only mesh colliders can be convex");
            return;
        }

        ::Entity raw_e = { owner.id, static_cast<::Scene*>(owner.scene_ptr) };
        ::Collider_SetConvex(raw_e, is_convex);
    }



    // ==========================================
    // Animator Implementation
    // ==========================================

    void AnimatorComponent::SetSkeleton(void* raw_skeleton) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::Animator_SetSkeleton(raw, static_cast<::Skeleton*>(raw_skeleton));
    }

    void AnimatorComponent::SetClip(Prism::AnimationClip clip) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::Animator_SetClip(raw, static_cast<::AnimationClip*>(clip.GetRaw()));
    }



    // ==========================================
    // Bone Attachment Implementation
    // ==========================================

    void BoneAttachmentComponent::SetLocalOffset(const Prism::Vector3& position, const Prism::Vector3& rotationEuler, const Prism::Vector3& scale) {
        Prism::Quaternion rot = Prism::Quaternion::FromEuler(rotationEuler);
        this->local_offset = Prism::Matrix4::CreateTransform(position, rot, scale);
    }



    // ==========================================
    // Line Renderer Implementation
    // ==========================================

    void LineRendererComponent::AddPoint(const Prism::Vector3& point) {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::LineRenderer_AddPoint(&static_cast<::Scene*>(e.scene)->line_renderers[e.id], ::Vector3{point.x, point.y, point.z});
    }

    void LineRendererComponent::SetPoint(uint32_t index, const Prism::Vector3& point) {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::LineRenderer_SetPoint(&static_cast<::Scene*>(e.scene)->line_renderers[e.id], index, ::Vector3{point.x, point.y, point.z});
    }

    void LineRendererComponent::SetPoints(const std::vector<Prism::Vector3>& points) {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::LineRenderer_SetPoints(&static_cast<::Scene*>(e.scene)->line_renderers[e.id], (::Vector3*)points.data(), (uint32_t)points.size());
    }

    uint32_t LineRendererComponent::GetPointCount() {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::LineRendererComponent* LR = &static_cast<::Scene*>(e.scene)->line_renderers[e.id];
        return LR->point_count;
    }

    void LineRendererComponent::ClearPoints() {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::LineRenderer_ClearPoints(&static_cast<::Scene*>(e.scene)->line_renderers[e.id]);
    }

    Prism::Vector3 LineRendererComponent::GetPoint(uint32_t index) const {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::Vector3 p = ::LineRenderer_GetPoint(&static_cast<::Scene*>(e.scene)->line_renderers[e.id], index);
        return Prism::Vector3(p.x, p.y, p.z);
    }

    std::vector<Prism::Vector3> LineRendererComponent::GetPoints() const {
        ::LineRendererComponent* c_line = &static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id];
        std::vector<Prism::Vector3> result(c_line->point_count);
        for(uint32_t i = 0; i < c_line->point_count; i++)
            result[i] = Prism::Vector3(c_line->points[i].x, c_line->points[i].y, c_line->points[i].z);
        return result;
    }

    void LineRendererComponent::SetMaterial(Prism::Material mat) {
        static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id].material = (::Material*)mat.GetRaw();
    }



    // ==========================================
    // Sprite Renderer Implementation
    // ==========================================

    void SpriteRendererComponent::SetSprite(const Prism::Texture sprite) {
        ::SpriteRenderer_SetSprite(reinterpret_cast<::SpriteRendererComponent*>(this), static_cast<::Texture*>(sprite.GetRaw()));
    }



    // ==========================================
    // Reflection Probe Component Implementation
    // ==========================================

    void ReflectionProbeComponent::SetBoxExtents(const Prism::Vector3& extents) {
        ::ReflectionProbe_SetBoxExtents(reinterpret_cast<::ReflectionProbeComponent*>(this), ::Vector3{extents.x, extents.y, extents.z} );
    }
    void ReflectionProbeComponent::SetBlendDistance(float distance) {
        ::ReflectionProbe_SetBlendDistance(reinterpret_cast<::ReflectionProbeComponent*>(this), distance);
    }
    void ReflectionProbeComponent::SetPriority(int32_t new_priority) {
        ::ReflectionProbe_SetPriority(reinterpret_cast<::ReflectionProbeComponent*>(this), new_priority);
    }
    void ReflectionProbeComponent::SetCaptureResolution(uint32_t resolution) {
        ::ReflectionProbe_SetCaptureResolution(reinterpret_cast<::ReflectionProbeComponent*>(this), resolution);
    }
    void ReflectionProbeComponent::MarkDirty() {
        ::ReflectionProbe_MarkDirty(reinterpret_cast<::ReflectionProbeComponent*>(this));
    }



    // ==========================================
    // UI Canvas Component Implementation
    // ==========================================

    void UICanvasComponent::SetActive(bool active) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::UICanvas_SetActive(raw, active);
    }
    void UICanvasComponent::SetScaleMode(UICanvasScaleMode mode) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::UICanvas_SetScaleMode(raw, static_cast<::UICanvasScaleMode>(mode));
    }
    void UICanvasComponent::SetReferenceResolution(const Prism::Vector2& resolution) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::UICanvas_SetReferenceResolution(raw, ::Vector2{resolution.x, resolution.y});
    }
    void UICanvasComponent::SetMatchWidthOrHeight(float match) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::UICanvas_SetMatchWidthOrHeight(raw, match);
    }



    // ==========================================
    // Rect Transform Component Implementation
    // ==========================================

    void RectTransformComponent::SetAnchoredPosition(const Prism::Vector2& position) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::RectTransform_SetAnchoredPosition(raw, ::Vector2{position.x, position.y});
    }
    void RectTransformComponent::SetSizeDelta(const Prism::Vector2& size) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::RectTransform_SetSizeDelta(raw, ::Vector2{size.x, size.y});
    }
    void RectTransformComponent::SetAnchors(const Prism::Vector2& min, const Prism::Vector2& max) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::RectTransform_SetAnchors(raw, ::Vector2{min.x, min.y}, ::Vector2{max.x, max.y});
    }
    void RectTransformComponent::SetPivot(const Prism::Vector2& pivot) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::RectTransform_SetPivot(raw, ::Vector2{pivot.x, pivot.y});
    }
    void RectTransformComponent::MarkDirty() {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::RectTransform_MarkDirty(raw);
    }



    // ==========================================
    // UI Text Component Implementation
    // ==========================================

    void UITextComponent::SetText(const std::string& value) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::UIText_SetText(raw, value.c_str());
    }

    void UITextComponent::SetFont(Prism::Font font) {
        ::Entity raw = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::UIText_SetFont(raw, static_cast<::Font*>(font.GetRaw()));
    }
}