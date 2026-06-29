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
    // Render Component Implementation
    // ==========================================

    void LightComponent::SetType(LightType type) {
        this->type = type;
    }
    void LightComponent::SetColor(const Prism::Color& color) {
        this->color = color;
    }
    void LightComponent::SetIntensity(float intensity) {
        this->intensity = intensity;
    }
    void LightComponent::SetAmbientStrength(float ambient_strength) {
        this->ambient_strength = ambient_strength;
    }
    void LightComponent::SetAttenuation(float constant, float linear, float quadratic) {
        this->constant = constant;
        this->linear = linear;
        this->quadratic = quadratic;
    }
    void LightComponent::SetSpotAngles(float inner_cutoff_degrees, float outer_cutoff_degrees) {
        this->inner_cut_off = inner_cutoff_degrees;
        this->outer_cut_off = outer_cutoff_degrees;
    }
    void LightComponent::SetShadowBoxSize(float half_extent) {
        this->shadow_box_size = half_extent;
    }
    float LightComponent::GetShadowBoxSize() const {
        return this->shadow_box_size;
    }

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
    uint8_t LightComponent::GetShadowCascadeCount() const {
        return this->shadow_cascade_count;
    }
    float LightComponent::GetShadowMaxDistance() const {
        return this->shadow_max_distance;
    }
    float LightComponent::GetCascadeSplitLambda() const {
        return this->cascade_split_lambda;
    }
    float LightComponent::GetCascadeBlendFraction() const {
        return this->cascade_blend_fraction;
    }



    // ==========================================
    // Mesh Renderer Component Implementation
    // ==========================================

    void MeshRendererComponent::SetMaterial(Prism::Material material) {
        this->raw_material_ptr = material.GetRaw();
    }
    void MeshRendererComponent::SetLayerMask(uint8_t layer_index) {
        this->layer_mask = (1u << layer_index); // Sets the object to a specific layer (0 through 31)
    }



    // ==========================================
    // Skinned Mesh Renderer Component Implementation
    // ==========================================

    void SkinnedMeshRendererComponent::SetMaterial(Prism::Material material) {
        this->raw_material_ptr = material.GetRaw();
    }
    void SkinnedMeshRendererComponent::SetLayerMask(uint8_t layer_index) {
        this->layer_mask = (1u << layer_index); // Sets the object to a specific layer (0 through 31)
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
    void CameraComponent::SetViewport(float x, float y, float width, float height) {
        this->viewport_x = x;
        this->viewport_y = y;
        this->viewport_width = width;
        this->viewport_height = height;
    }
    void CameraComponent::SetFOV(float fov) {
        this->fov = fov;
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

    void LineRendererComponent::SetThickness(float startThickness, float endThickness) {
        ::LineRendererComponent* c_line = &static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id];
        c_line->start_thickness = startThickness;
        c_line->end_thickness = endThickness;
    }

    void LineRendererComponent::SetThickness(float thickness) {
        SetThickness(thickness, thickness);
    }

    void LineRendererComponent::SetColor(const Prism::Color& color) {
        static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id].color = ::Color{color.r, color.g, color.b, color.a};
    }

    void LineRendererComponent::SetUseWorldSpace(bool useWorldSpace) {
        static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id].use_world_space = useWorldSpace;
    }

    bool LineRendererComponent::GetUseWorldSpace() const {
        return static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id].use_world_space;
    }

    void LineRendererComponent::SetLoop(bool isLoop) {
        static_cast<::Scene*>(this->entity.scene_ptr)->line_renderers[this->entity.id].is_loop = isLoop;
    }



    // ==========================================
    // Sprite Renderer Implementation
    // ==========================================

    void SpriteRendererComponent::SetColor(const Prism::Color& color) {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        static_cast<::Scene*>(e.scene)->sprite_renderers[e.id].color = ::Color{color.r, color.g, color.b, color.a};
    }
    Prism::Color SpriteRendererComponent::GetColor() const {
        ::Entity e = { this->entity.id, static_cast<::Scene*>(this->entity.scene_ptr) };
        ::Color c = static_cast<::Scene*>(e.scene)->sprite_renderers[e.id].color;
        return Prism::Color(c.r, c.g, c.b, c.a);
    }
}