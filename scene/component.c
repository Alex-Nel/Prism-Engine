#include "scene.h"





// Sets gravity for this engine and physics engine
void Rigidbody_SetGravity(Entity entity, bool use_gravity)
{
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    ColliderComponent* col = Entity_GetCollider(entity);

    if (rb && col && col->physics_handle) 
    {
        rb->use_gravity = use_gravity;    
        
        Physics_SetGravityState(entity.scene->physics_world, col->physics_handle, use_gravity);
    }
}





// Sets an entities rigidbody to kinematic or not
void Rigidbody_SetKinematic(Entity entity, bool is_kinematic)
{
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    ColliderComponent* col = Entity_GetCollider(entity);

    if (rb && col && col->physics_handle) 
    {
        rb->is_kinematic = is_kinematic;
        Physics_SetKinematicState(entity.scene->physics_world, col->physics_handle, is_kinematic);
    }
}





// Sets the linear velocity of a rigidbody
void Rigidbody_SetLinearVelocity(Entity entity, Vector3 velocity)
{
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    ColliderComponent* col = Entity_GetCollider(entity);

    if (rb)
    {
        Physics_SetLinearVelocity(col->physics_handle, velocity);
    }
}





// Moves a rigidbody to a specific target position
void Rigidbody_MovePosition(Entity entity, Vector3 position)
{
    if (!Entity_IsValid(entity))
        return;
    
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    ColliderComponent* col = Entity_GetCollider(entity);
    Transform* t = Entity_GetTransform(entity);

    if (!rb || !col || !col->physics_handle || !t)
        return;
    
    if (rb->is_kinematic)
    {
        // Kinematic approach: Set the position directly in the physics engine
        Physics_SetBodyPosition(col->physics_handle, position);

        // Update local transform as well
        t->local_position = position;
    }
    else
    {
        // Dynamic approach: calculate the exact velocity needed to travel from current location to the target position in one physics step
        float fixed_dt = Time_FixedDeltaTime();

        if (fixed_dt > 0.0f)
        {
            Vector3 required_velocity = {
                (position.x - t->local_position.x) / fixed_dt,
                (position.y - t->local_position.y) / fixed_dt,
                (position.z - t->local_position.z) / fixed_dt
            };

            // Push it using physics, it will collide naturally on the way
            Physics_SetLinearVelocity(col->physics_handle, required_velocity);
        }
    }
}





// Sets an entities collider with one collision layer and mask (several layers OR'd together)
void Collider_SetLayerAndMask(Entity entity, CollisionLayer layer, int mask)
{
    ColliderComponent* c = Entity_GetCollider(entity);
    if (c && c->physics_handle)
    {
        c->collision_layer = layer;
        c->collision_mask = mask;
        Physics_SetCollisionFilter(entity.scene->physics_world, c->physics_handle, layer, mask);
    }
}





// Sets the extents of a box collider
void Collider_SetBoxExtents(Entity entity, Vector3 new_extents)
{
    ColliderComponent* c = Entity_GetCollider(entity);

    if (!c) return;

    if (c->type != COLLIDER_BOX)
    {
        Log_Warning("Attempting to set box extents for a non-box collider");
        return;
    }

    c->extents = new_extents;

    Physics_SetBoxExtents(c->physics_handle, new_extents);

    // If this entity has a rigidbody, we must recalculate its mass distribution
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    if (rb && !rb->is_kinematic && rb->mass > 0.0f)
    {
        Physics_RecalculateMass(c->physics_handle, rb->mass);
    }
}





// Sets the radius of a sphere collider
void Collider_SetSphereRadius(Entity entity, float new_radius)
{
    ColliderComponent* c = Entity_GetCollider(entity);

    if (!c) return;

    if (c->type != COLLIDER_SPHERE)
    {
        Log_Warning("Attempting to set radius for a non-sphere collider");
        return;
    }

    c->radius = new_radius;

    Physics_SetSphereRadius(c->physics_handle, new_radius);

    // Fix the mass
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    if (rb && !rb->is_kinematic && rb->mass > 0.0f)
    {
        Physics_RecalculateMass(c->physics_handle, rb->mass);
    }
}





// Sets the scale of a mesh collider
void Collider_SetMeshScale(Entity entity, Vector3 new_scale)
{
    ColliderComponent* c = Entity_GetCollider(entity);
    if (!c) return;

    if (c->type != COLLIDER_MESH)
    {
        Log_Warning("Attempting to set mesh scale for a non-mesh collider");
        return;
    }

    // Update the ECS state
    c->mesh_scale = new_scale;

    // Tell Bullet to scale the mesh
    Physics_SetMeshScale(c->physics_handle, new_scale);

    // Mesh colliders should usually be static, but update just in case (undefined bahavior)
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    if (rb && !rb->is_kinematic && rb->mass > 0.0f)
    {
        Physics_RecalculateMass(c->physics_handle, rb->mass);
    }
}





// Safely swaps a mesh collider between Convex and Non-Convex at runtime
void Collider_SetConvex(Entity entity, bool is_convex)
{
    if (!Entity_IsValid(entity)) return;

    ColliderComponent* c = &entity.scene->colliders[entity.id];

    // Safety checks
    if (c->type != COLLIDER_MESH) return;
    if (c->is_convex == is_convex) return; // No change needed

    // Destroy the old physics body
    if (c->physics_handle)
        Physics_DestroyBody(entity.scene->physics_world, c->physics_handle);

    // Update internal flag
    c->is_convex = is_convex;

    // Rebuild the physics body using the Asset Manager pointers
    Transform* t = &entity.scene->transforms[entity.id];
    
    if (is_convex)
    {
        c->physics_handle = Physics_CreateConvexCollider(
            entity.scene->physics_world, entity.id, t->local_position,
            c->mesh_ptr->vertices, sizeof(Vertex3D), c->mesh_ptr->vertex_count, c->is_trigger
        );
    }
    else
    {
        c->physics_handle = Physics_CreateMeshCollider(
            entity.scene->physics_world, entity.id, t->local_position,
            c->mesh_ptr->vertices, sizeof(Vertex3D), c->mesh_ptr->vertex_count,
            c->mesh_ptr->indices, c->mesh_ptr->index_count, c->is_trigger
        );
    }

    // Restore the base Collider properties
    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);
    Physics_SetCollisionFilter(entity.scene->physics_world, c->physics_handle, c->collision_layer, c->collision_mask);

    // Restore the Rigidbody properties (if the entity has one)
    if (entity.scene->component_masks[entity.id] & COMPONENT_RIGIDBODY)
    {
        RigidbodyComponent* rb = &entity.scene->rigidbodies[entity.id];
        
        // Non-convex meshes CANNOT have mass.
        if (!is_convex) 
        {
            Log_Warning("WARNING: Swapping to Non-Convex Mesh. Forcing Rigidbody mass to 0 (Static).");
            rb->mass = 0.0f;
        }

        // Push all the saved states back into the fresh Bullet object
        Physics_AddRigidbody(entity.scene->physics_world, c->physics_handle, rb->mass);
        Physics_SetDamping(c->physics_handle, rb->linear_drag, rb->angular_drag);
        Physics_SetGravityState(entity.scene->physics_world, c->physics_handle, rb->use_gravity);
        Physics_SetRotationConstraints(c->physics_handle, rb->freeze_rot_x, rb->freeze_rot_y, rb->freeze_rot_z);
        Physics_SetKinematicState(entity.scene->physics_world, c->physics_handle, rb->is_kinematic);
    }
}















// Recalculates a cameras projection matrix when needed
static void Camera_RecalculateProjectionIfNeeded(CameraComponent* cam)
{
    // If the fov is 0, it means the camera component was just created and is uninitialized.
    if (cam->fov <= 0.0f) {
        cam->fov = 60.0f;
        cam->nearZ = 0.1f;
        cam->farZ = 1000.0f;
        cam->is_dirty = true;
    }

    if (cam->is_dirty && cam->viewport_height > 0)
    {
        float aspect = (float)cam->viewport_width / (float)cam->viewport_height;
        cam->projection_matrix = Matrix4Perspective(cam->fov, aspect, cam->nearZ, cam->farZ);
        cam->is_dirty = false;
    }
}





// Converts a point on the screen into a ray
Ray Camera_ScreenPointToRay(CameraComponent* cam, Transform* cam_transform, float mouse_x, float mouse_y)
{
    // If the cam or transform are not valid, return default ray
    if (!cam || !cam_transform)
        return (Ray) { {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} };

    
    // Prevent Viewport Division by Zero!
    if (cam->viewport_width == 0 || cam->viewport_height == 0) 
        return (Ray){ Transform_GetGlobalPosition(cam_transform), {0.0f, 0.0f, 1.0f} };

    // If the matrix is empty, rebuild it
    Camera_RecalculateProjectionIfNeeded(cam);

    // Convert Screen Coordinates to Normalized Device Coordinates
    // Assuming (0,0) is top-left of the window
    float x = (2.0f * (mouse_x - (float)cam->viewport_x)) / (float)cam->viewport_width - 1.0f;
    float y = 1.0f - (2.0f * (mouse_y - (float)cam->viewport_y)) / (float)cam->viewport_height;

    // Clip Space
    Vector4 near_clip = { x, y, 0.0001f, 1.0f }; 
    Vector4 far_clip  = { x, y, 0.9999f, 1.0f };
    
    // Eye Space (Reverse the Projection)
    Matrix4 inv_proj = Matrix4Inverse(cam->projection_matrix);
    Vector3 cam_pos = Transform_GetGlobalPosition(cam_transform);
    Matrix4 view_matrix = Matrix4CreateView(cam_pos, cam_transform->local_rotation);
    Matrix4 inv_view = Matrix4Inverse(view_matrix); // Restore Inverse View!
    
    // 4. Unproject Near Point
    Vector4 near_eye = Matrix4MultiplyVector4(inv_proj, near_clip);
    if (near_eye.w != 0.0f) {
        near_eye.x /= near_eye.w; near_eye.y /= near_eye.w; near_eye.z /= near_eye.w; near_eye.w = 1.0f;
    }
    Vector4 near_world = Matrix4MultiplyVector4(inv_view, near_eye);

    // 5. Unproject Far Point
    Vector4 far_eye = Matrix4MultiplyVector4(inv_proj, far_clip);
    if (far_eye.w != 0.0f) {
        far_eye.x /= far_eye.w; far_eye.y /= far_eye.w; far_eye.z /= far_eye.w; far_eye.w = 1.0f;
    }
    Vector4 far_world = Matrix4MultiplyVector4(inv_view, far_eye);

    // 6. Direction = Far - Near
    Vector3 ray_dir = {
        far_world.x - near_world.x,
        far_world.y - near_world.y,
        far_world.z - near_world.z
    };
    
    return (Ray){ cam_pos, Vector3Normalize(ray_dir) };
}





// Converts a 3D world position into a 2D pixel coordinate on the screen
Vector2 Camera_WorldToScreenPoint(CameraComponent* cam, Transform* cam_transform, Vector3 world_pos)
{
    // If the cam or transform are not valid, return default ray
    if (!cam || !cam_transform)
        return (Vector2){ 0.0f, 0.0f };

    // Prevent Viewport Division by Zero!
    if (cam->viewport_width == 0 || cam->viewport_height == 0) 
        return (Vector2){ 0.0f, 0.0f };

    // If the matrix is empty, rebuild it
    Camera_RecalculateProjectionIfNeeded(cam);
    
    // 1. Generate the exact View Matrix the renderer uses
    Vector3 cam_pos = Transform_GetGlobalPosition(cam_transform);
    Matrix4 view_matrix = Matrix4CreateView(cam_pos, cam_transform->local_rotation);
    
    // 2. Transform World to Eye Space
    Vector4 clip_space = {world_pos.x, world_pos.y, world_pos.z, 1.0f};
    clip_space = Matrix4MultiplyVector4(view_matrix, clip_space);

    // 3. Transform Eye to Clip Space
    clip_space = Matrix4MultiplyVector4(cam->projection_matrix, clip_space);

    // 4. Frustum Culling 
    // If w is less than a tiny positive number, it is touching the camera lens or behind it!
    if (clip_space.w < 0.0001f)
        return (Vector2){ -9999.0f, -9999.0f };

    // 5. Perspective Divide (Normalized Device Coordinates: -1 to 1)
    Vector3 ndc;
    ndc.x = clip_space.x / clip_space.w;
    ndc.y = clip_space.y / clip_space.w;
    
    // 6. Convert NDC to Screen Pixels
    Vector2 screen_pos;
    screen_pos.x = (float)cam->viewport_x + ((ndc.x + 1.0f) / 2.0f) * (float)cam->viewport_width;
    screen_pos.y = (float)cam->viewport_y + ((1.0f - ndc.y) / 2.0f) * (float)cam->viewport_height;

    return screen_pos;
}





// Sets the FOV of a camera
void Camera_SetFOV(CameraComponent* cam, float FOV)
{
    if (!cam)
        return;

    cam->fov = FOV;
}















// Sets the specific material slot with a chosen material
void Renderable_SetMaterial(RenderComponent* r, Material* material)
{
    if (!r)
    {
        Log_Error("Renderable does not exist");
        return;
    }

    r->material = material;
}















// Adds a point to a line renderer
void LineRenderer_AddPoint(LineRendererComponent* line, Vector3 point)
{
    if (!line || !line->is_active)
        return;

    if (line->point_count < MAX_LINE_POINTS)
        line->points[line->point_count++] = point;
}





// Clears all the points in a line renderer
void LineRenderer_ClearPoints(LineRendererComponent* line)
{
    if (!line || !line->is_active)
        return;

    line->point_count = 0;
}





// Sets a point in the line renderer to a specific Vector3
void LineRenderer_SetPoint(LineRendererComponent* line, uint32_t index, Vector3 point)
{
    if (!line || !line->is_active || index >= line->point_count)
        return;

    line->points[index] = point;
}





// Gets a specific point from a line renderer
Vector3 LineRenderer_GetPoint(LineRendererComponent* line, uint32_t index)
{
    if (!line || !line->is_active || index >= line->point_count)
        return (Vector3){0,0,0};

    return line->points[index];
}





// Sets all point of a line renderer to a given array
void LineRenderer_SetPoints(LineRendererComponent* line, Vector3* points, uint32_t count)
{
    if (!line || !line->is_active || !points)
        return;
     
    if (count > MAX_LINE_POINTS)
        line->point_count = MAX_LINE_POINTS;
    else
        line->point_count = count;
    
    for (uint32_t i = 0; i < line->point_count; i++)
        line->points[i] = points[i];
}