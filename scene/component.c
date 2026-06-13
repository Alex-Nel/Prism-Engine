#include "scene.h"
#include <string.h>




// Sets the name of an entity
void Entity_SetName(Entity entity, const char* name)
{
    if (!entity.scene || !name) return;

    // Copy the string into the ECS array
    strncpy(entity.scene->names[entity.id].name, name, MAX_NAME_LENGTH - 1);
    
    // Guarantee it is null-terminated so printf and strcmp don't crash
    entity.scene->names[entity.id].name[MAX_NAME_LENGTH - 1] = '\0'; 

    // Tell ECS this entity has a name
    entity.scene->component_masks[entity.id] |= COMPONENT_NAME;
}





// Adds a transform to an entity with a specified position, rotation, and scale
void Entity_AddTransform(Entity entity, Vector3 position, Quaternion rotation, Vector3 scale)
{
    if (!entity.scene) return;

    Transform* t = &entity.scene->transforms[entity.id];
    t->local_position = position;
    t->local_rotation = rotation;
    t->local_scale = scale;

    t->local_rotation_euler = (Vector3){0.0f, 0.0f, 0.0f};

    t->world_matrix = Matrix4Identity();

    // Set entity to child of the scene
    t->parent_id = ENTITY_NONE;
    t->first_child_id = ENTITY_NONE;
    t->next_sibling_id = ENTITY_NONE;
    t->prev_sibling_id = ENTITY_NONE;

    t->is_dirty = true;

    // Register the component
    entity.scene->component_masks[entity.id] |= COMPONENT_TRANSFORM;
}





// Adds a renderable component to an entity with a specified mesh and material
void Entity_AddRenderable(Entity entity, Mesh* mesh, Material* material)
{
    if (!entity.scene) return;

    RenderComponent* r = &entity.scene->renderables[entity.id];
    r->mesh = mesh;
    r->material = material;

    entity.scene->component_masks[entity.id] |= COMPONENT_RENDER;
}





// Adds a camera component to an entity with a specified FOX, near clipping plane, and far clipping plane
void Entity_AddCamera(Entity entity, float fov, float nearZ, float farZ)
{
    if (!entity.scene) return;

    CameraComponent* cam = &entity.scene->cameras[entity.id];
    cam->fov = fov;
    cam->nearZ = nearZ;
    cam->farZ = farZ;
    cam->is_dirty = true;

    entity.scene->component_masks[entity.id] |= COMPONENT_CAMERA;
}





// Adds a point light component to an entity with specified light attributes
void Entity_AddPointLight(Entity entity, Color color, float intensity, float constant, float linear, float quadratic)
{
    if (!entity.scene) return;
    
    PointLightComponent* light = &entity.scene->point_lights[entity.id];
    light->color = color;
    light->intensity = intensity;
    light->constant = constant;
    light->linear = linear;
    light->quadratic = quadratic;
    
    entity.scene->component_masks[entity.id] |= COMPONENT_POINT_LIGHT;
}





// Adds a box collider to an entity
void Entity_AddColliderBox(Entity entity, Vector3 extents, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->owner = entity;
    c->type = COLLIDER_BOX;
    c->is_trigger = is_trigger;
    c->mesh_ptr = NULL;
    c->extents = extents;
    c->radius = 0.0f;
    Transform* t = &entity.scene->transforms[entity.id];
    c->mesh_scale = (Vector3){1.0f, 1.0f, 1.0f};

    // Creates a static box for the physics engine
    c->physics_handle = Physics_CreateBoxCollider(entity.scene->physics_world, entity.id, t->local_position, extents, is_trigger);
    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);

    // Push default layers/mask to Bullet
    c->collision_layer = COLLISION_LAYER_DEFAULT;
    c->collision_mask = COLLISION_MASK_ALL;
    Physics_SetCollisionFilter(entity.scene->physics_world, c->physics_handle, c->collision_layer, c->collision_mask);

    entity.scene->component_masks[entity.id] |= COMPONENT_COLLIDER;
}





// Adds a box collider while automatically finding mesh extends
void Entity_AddColliderBoxAuto(Entity entity, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    // Ensure the entity actually has a mesh to read
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_RENDER))
    {
        Log_Warning("WARNING: Cannot Auto-Fit Box Collider. Entity has no Mesh!");
        return;
    }

    RenderComponent* r = &entity.scene->renderables[entity.id];

    if (r->mesh)
    {
        AABB bounds = r->mesh->local_bounds;

        // Calculate the extents (Half-Size) from the local bounds
        Vector3 extents = {
            (bounds.max.x - bounds.min.x),
            (bounds.max.y - bounds.min.y),
            (bounds.max.z - bounds.min.z)
        };

        Entity_AddColliderBox(entity, extents, is_trigger);
    }
}





// Adds a sphere collider to an entity
void Entity_AddColliderSphere(Entity entity, float radius, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->owner = entity;
    c->type = COLLIDER_SPHERE;
    c->is_trigger = is_trigger;
    c->mesh_ptr = NULL;
    c->extents = (Vector3){0, 0, 0};
    c->radius = radius;
    Transform* t = &entity.scene->transforms[entity.id];
    c->mesh_scale = (Vector3){1.0f, 1.0f, 1.0f};

    // Creates a static sphere
    c->physics_handle = Physics_CreateSphereCollider(entity.scene->physics_world, entity.id, t->local_position, radius, is_trigger);
    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);

    // Push default layers/mask to Bullet
    c->collision_layer = COLLISION_LAYER_DEFAULT;
    c->collision_mask = COLLISION_MASK_ALL;
    Physics_SetCollisionFilter(entity.scene->physics_world, c->physics_handle, c->collision_layer, c->collision_mask);

    entity.scene->component_masks[entity.id] |= COMPONENT_COLLIDER;
}





// TODO: Make colliders follow models too
// Adds a collider to an entity that matches its mesh
void Entity_AddColliderMesh(Entity entity, Mesh* mesh, bool is_trigger, bool is_convex)
{
    if (!Entity_IsValid(entity)) return;
    
    if (!mesh)
    {
        Log_Error("CRITICAL: Tried to add Mesh Collider to an invalid mesh.");
        return;
    }

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->owner = entity;
    c->type = COLLIDER_MESH;
    c->is_trigger = is_trigger;
    c->is_convex = is_convex;
    c->mesh_ptr = mesh;
    c->extents = (Vector3){0, 0, 0};
    c->radius = 0.0f;

    Transform* t = &entity.scene->transforms[entity.id];
    c->mesh_scale = t->local_scale;

    if (is_convex)
    {
        c->physics_handle = Physics_CreateConvexCollider(
            entity.scene->physics_world, entity.id, t->local_position,
            mesh->vertices, sizeof(Vertex3D), mesh->vertex_count, is_trigger
        );
    }
    else
    {
        c->physics_handle = Physics_CreateMeshCollider(
            entity.scene->physics_world, entity.id, t->local_position,
            mesh->vertices, sizeof(Vertex3D), mesh->vertex_count,
            mesh->indices, mesh->index_count, is_trigger
        );
    }

    // Set the physics bodies scale and rotation
    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);

    // Push default layers/mask to Bullet
    c->collision_layer = COLLISION_LAYER_DEFAULT;
    c->collision_mask = COLLISION_MASK_ALL;
    Physics_SetCollisionFilter(entity.scene->physics_world, c->physics_handle, c->collision_layer, c->collision_mask);

    entity.scene->component_masks[entity.id] |= COMPONENT_COLLIDER;
}





// Adds a rigidbody to an entity with a specified mass
void Entity_AddRigidbody(Entity entity, float mass)
{
    if (!Entity_IsValid(entity)) return;

    // If the entity doesn't have a collider, return
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_COLLIDER))
    {
        Log_Warning("WARNING: Entity must have a Collider BEFORE adding a Rigidbody!\n");
        return;
    }

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    
    // Don't let users make Complex Meshes (not convex) dynamic
    if (c->type == COLLIDER_MESH && !c->is_convex)
    {
        Log_Warning("WARNING: Non-convex Mesh Colliders cannot be Rigidbodies. Ignoring.\n");
        return;
    }

    RigidbodyComponent* rb = &entity.scene->rigidbodies[entity.id];
    rb->owner = entity;
    rb->mass = mass;
    rb->linear_drag = 0.0f;
    rb->angular_drag = 0.05f; // A bit of rotational drag looks more realistic
    rb->use_gravity = true;
    rb->is_kinematic = false;
    rb->freeze_rot_x = false;
    rb->freeze_rot_y = false;
    rb->freeze_rot_z = false;

    // Upgrade the static collider into a dynamic falling physics object!
    Physics_AddRigidbody(entity.scene->physics_world, c->physics_handle, mass);
    Physics_SetDamping(c->physics_handle, rb->linear_drag, rb->angular_drag);
    Physics_SetGravityState(entity.scene->physics_world, c->physics_handle, rb->use_gravity);
    Physics_SetRotationConstraints(c->physics_handle, rb->freeze_rot_x, rb->freeze_rot_y, rb->freeze_rot_z);

    entity.scene->component_masks[entity.id] |= COMPONENT_RIGIDBODY;
}





// Adds an audio listener to an entity
void Entity_AddAudioListener(Entity entity)
{
    if (!Entity_IsValid(entity)) return;
    
    AudioListenerComponent* listener = &entity.scene->audio_listeners[entity.id];
    listener->active = true;

    entity.scene->component_masks[entity.id] |= COMPONENT_AUDIO_LISTENER;
}





// Adds an audio source to an entity
void Entity_AddAudioSource(Entity entity)
{
    if (!Entity_IsValid(entity)) return;
    
    AudioSourceComponent* src = &entity.scene->audio_sources[entity.id];
    src->clip = (AudioClipHandle){0};
    src->volume = 1.0f;
    src->pitch = 1.0f;
    src->loop = false;
    src->play_on_awake = false;
    src->is_playing = false;
    src->is_spatial = true;
    src->min_distance = 1.0f;
    src->max_distance = 50.0f;

    entity.scene->component_masks[entity.id] |= COMPONENT_AUDIO_SOURCE;
}





// Adds a custom script to an entity
void Entity_BindScript(Entity entity, ScriptInstance new_script)
{
    if (!entity.scene) return;
    
    ScriptComponent* script_comp = &entity.scene->scripts[entity.id];
    
    // Check if the number of custom script has been maxed out
    if (script_comp->count >= MAX_SCRIPTS_PER_ENTITY)
    {
        Log_Error("WARNING: Entity %d exceeded maximum scripts (%d)!\n", entity.id, MAX_SCRIPTS_PER_ENTITY);
        return;
    }
    
    // Get the next available slot
    uint32_t index = script_comp->count;

    // Slot the new script in
    script_comp->instances[index] = new_script;
    script_comp->instances[index].has_started = false;
    
    // Increment the counter
    script_comp->count++;
    
    // Tell the ECS this entity has at least one script
    entity.scene->component_masks[entity.id] |= COMPONENT_SCRIPT;
}















// Returns the name of an entity
const char* Entity_GetName(Entity entity)
{
    if (!entity.scene)
        return NULL;

    return entity.scene->names[entity.id].name;
}





// Returns an entities transform struct
Transform* Entity_GetTransform(Entity entity)
{
    if (!entity.scene) return NULL;
    
    // Ensure the entity actually has a transform before returning a pointer to it
    if ((entity.scene->component_masks[entity.id] & COMPONENT_TRANSFORM) == COMPONENT_TRANSFORM)
    {
        return &entity.scene->transforms[entity.id];
    }
    
    return NULL;
}





// Returns an entities Renderable struct
RenderComponent* Entity_GetRenderable(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_RENDER) == COMPONENT_RENDER)
    {
        return &entity.scene->renderables[entity.id];
    }
    
    return NULL;
}





// Returns an entities Mesh Handle
Mesh* Entity_GetMesh(Entity entity)
{
    if (!Entity_IsValid(entity)) return NULL;
    
    // Check if the entity actually has a render component
    if (entity.scene->component_masks[entity.id] & COMPONENT_RENDER)
        return entity.scene->renderables[entity.id].mesh;
    
    return NULL;
}





// Returns an entities camera struct
CameraComponent* Entity_GetCamera(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_CAMERA) == COMPONENT_CAMERA)
    {
        return &entity.scene->cameras[entity.id];
    }

    return NULL;
}





// Returns an entities point light struct
PointLightComponent* Entity_GetPointLight(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_POINT_LIGHT) == COMPONENT_POINT_LIGHT)
    {
        return &entity.scene->point_lights[entity.id];
    }

    return NULL;
}





// Returns an entities collider struct (Regardless of type)
ColliderComponent* Entity_GetCollider(Entity entity)
{
    if (!Entity_IsValid(entity)) return NULL;
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_COLLIDER)) return NULL;
    
    return &entity.scene->colliders[entity.id];
}





// Returns an entities Rigibody struct
RigidbodyComponent* Entity_GetRigidbody(Entity entity)
{
    if (!Entity_IsValid(entity)) return NULL;
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_RIGIDBODY)) return NULL;
    
    return &entity.scene->rigidbodies[entity.id];
}





// Returns an entities audio listener
AudioListenerComponent* Entity_GetAudioListener(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_AUDIO_LISTENER) == COMPONENT_AUDIO_LISTENER)
    {
        return &entity.scene->audio_listeners[entity.id];
    }

    return NULL;
}





// Returns an entities audio source
AudioSourceComponent* Entity_GetAudioSource(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_AUDIO_SOURCE) == COMPONENT_AUDIO_SOURCE)
    {
        return &entity.scene->audio_sources[entity.id];
    }

    return NULL;
}





// Returns the pointer to all the entities scripts
ScriptComponent* Entity_GetScripts(Entity entity)
{
    if (!Entity_IsValid(entity)) return NULL;
    
    // Check if the entity actually has any scripts
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_SCRIPT)) return NULL;

    return &entity.scene->scripts[entity.id];
}















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















// Converts a point on the screen into a ray
Ray Camera_ScreenPointToRay(Matrix4 projection, Matrix4 view, Vector3 camera_pos, float mouse_x, float mouse_y, float screen_width, float screen_height)
{
    // If the projection matrix is empty, return an empty ray to prevent NaN crash
    if (projection.m0 == 0.0f && projection.m5 == 0.0f)
        return (Ray){ camera_pos, {0.0f, 0.0f, 1.0f} }; 

    // Convert Screen Coordinates to Normalized Device Coordinates
    // Assuming (0,0) is top-left of the window
    float x = (2.0f * mouse_x) / screen_width - 1.0f;
    float y = 1.0f - (2.0f * mouse_y) / screen_height;
    
    // Clip Space
    Vector4 ray_clip = { x, y, -1.0f, 1.0f };
    
    // Eye Space (Reverse the Projection)
    Matrix4 inv_proj = Matrix4Inverse(projection);
    Vector4 ray_eye = Matrix4MultiplyVector4(inv_proj, ray_clip);
    ray_eye.z = -1.0f;
    ray_eye.w = 0.0f;
    
    // World Space (Reverse the View)
    // Matrix4 inv_view = Matrix4Inverse(view);
    Vector4 ray_world_4d = Matrix4MultiplyVector4(view, ray_eye);
    
    Vector3 ray_world = { ray_world_4d.x, ray_world_4d.y, ray_world_4d.z };
    ray_world = Vector3Normalize(ray_world);
    
    return (Ray){ camera_pos, ray_world };
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