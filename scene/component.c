#include "scene.h"
#include <string.h>




void Entity_SetName(Entity entity, const char* name)
{
    if (!entity.scene || !name) return;

    // Safely copy the string into the ECS array
    strncpy(entity.scene->names[entity.id].name, name, MAX_NAME_LENGTH - 1);
    
    // Guarantee it is null-terminated so printf and strcmp don't crash
    entity.scene->names[entity.id].name[MAX_NAME_LENGTH - 1] = '\0'; 

    // Tell the ECS this entity now has a name!
    entity.scene->component_masks[entity.id] |= COMPONENT_NAME;
}





void Entity_AddTransform(Entity entity, Vector3 position, Quaternion rotation, Vector3 scale)
{
    if (!entity.scene) return;

    Transform* t = &entity.scene->transforms[entity.id];
    t->local_position = position;
    t->local_rotation = rotation;
    t->local_scale = scale;

    t->local_rotation_euler = (Vector3){0.0f, 0.0f, 0.0f};

    t->world_matrix = Matrix4Identity();

    t->parent_id = ENTITY_NONE;
    t->first_child_id = ENTITY_NONE;
    t->next_sibling_id = ENTITY_NONE;
    t->prev_sibling_id = ENTITY_NONE;

    t->is_dirty = true;

    // Register the component
    entity.scene->component_masks[entity.id] |= COMPONENT_TRANSFORM;
}





// void Entity_AddRenderable(Entity entity, uint32_t mesh_id, uint32_t shader_id, uint32_t texture_id)
void Entity_AddRenderable(Entity entity, uint32_t mesh_id, uint32_t material_id)
{
    if (!entity.scene) return;

    RenderComponent* r = &entity.scene->renderables[entity.id];
    r->mesh_id = mesh_id;
    r->material_id = material_id;

    entity.scene->component_masks[entity.id] |= COMPONENT_RENDER;
}





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





void Entity_AddPointLight(Entity entity, Vector3 color, float intensity, float constant, float linear, float quadratic)
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





void Entity_AddColliderBox(Entity entity, Vector3 extents, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->type = COLLIDER_BOX;
    c->is_trigger = is_trigger;
    Transform* t = &entity.scene->transforms[entity.id];

    // Creates a STATIC box
    c->physics_handle = Physics_CreateBoxCollider(entity.scene->physics_world, t->local_position, extents, is_trigger);
    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);

    entity.scene->component_masks[entity.id] |= COMPONENT_COLLIDER;
}





void Entity_AddColliderBoxAuto(Entity entity, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    // 1. Ensure the entity actually has a mesh to read!
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_RENDER)) {
        printf("WARNING: Cannot Auto-Fit Box Collider. Entity has no Mesh!\n");
        return;
    }

    // 2. Grab the mesh blueprint from the Asset Manager
    MeshHandle mesh = { entity.scene->renderables[entity.id].mesh_id };
    MeshData* data = Asset_GetMeshData(mesh);

    if (data) {
        // 3. Calculate the extents (Half-Size) from the local bounds
        Vector3 extents = {
            (data->local_bounds.max.x - data->local_bounds.min.x) * 0.5f,
            (data->local_bounds.max.y - data->local_bounds.min.y) * 0.5f,
            (data->local_bounds.max.z - data->local_bounds.min.z) * 0.5f
        };

        // 4. Just pass the math to our standard manual function!
        Entity_AddColliderBox(entity, extents, is_trigger);
    }
}





void Entity_AddColliderSphere(Entity entity, float radius, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->type = COLLIDER_SPHERE;
    c->is_trigger = is_trigger;
    Transform* t = &entity.scene->transforms[entity.id];

    // Creates a STATIC sphere
    c->physics_handle = Physics_CreateSphereCollider(entity.scene->physics_world, t->local_position, radius, is_trigger);
    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);

    entity.scene->component_masks[entity.id] |= COMPONENT_COLLIDER;
}





void Entity_AddColliderMesh(Entity entity, MeshHandle mesh, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;
    
    MeshData* data = Asset_GetMeshData(mesh);
    if (!data)
    {
        Log_Error("CRITICAL: Tried to add Mesh Collider to invalid mesh handle!");
        return;
    }

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->type = COLLIDER_MESH;
    c->is_trigger = is_trigger;
    Transform* t = &entity.scene->transforms[entity.id];

    // Pass the raw memory pointers to Bullet so it can bake the BVH Tree!
    c->physics_handle = Physics_CreateMeshCollider(
        entity.scene->physics_world, t->local_position,
        data->vertices, sizeof(Vertex3D), data->vertex_count,
        data->indices, data->index_count, is_trigger
    );

    Physics_SetBodyScale(c->physics_handle, t->local_scale);
    Physics_SetBodyRotation(c->physics_handle, t->local_rotation);

    entity.scene->component_masks[entity.id] |= COMPONENT_COLLIDER;
}





void Entity_AddRigidbody(Entity entity, float mass)
{
    if (!Entity_IsValid(entity)) return;

    // You cannot have a Rigidbody without a Collider!
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_COLLIDER))
    {
        Log_Warning("WARNING: Entity must have a Collider BEFORE adding a Rigidbody!\n");
        return;
    }

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    
    // Safety check: Don't let users make Complex Meshes dynamic!
    if (c->type == COLLIDER_MESH)
    {
        Log_Warning("WARNING: Mesh Colliders cannot be Rigidbodies. Ignoring.\n");
        return;
    }

    RigidbodyComponent* rb = &entity.scene->rigidbodies[entity.id];
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
    // Physics_SetGravityState(c->physics_handle, rb->use_gravity);
    Physics_SetGravityState(entity.scene->physics_world, c->physics_handle, rb->use_gravity);
    Physics_SetRotationConstraints(c->physics_handle, rb->freeze_rot_x, rb->freeze_rot_y, rb->freeze_rot_z);

    entity.scene->component_masks[entity.id] |= COMPONENT_RIGIDBODY;
}





void Entity_BindScript(Entity entity, void* data, ScriptStartFunc start, ScriptUpdateFunc update, ScriptDestroyFunc destroy)
{
    if (!entity.scene) return;
    
    ScriptComponent* script_comp = &entity.scene->scripts[entity.id];
    
    // Check if we hit the limit!
    if (script_comp->count >= MAX_SCRIPTS_PER_ENTITY)
    {
        Log_Error("WARNING: Entity %d exceeded maximum scripts (%d)!\n", entity.id, MAX_SCRIPTS_PER_ENTITY);
        return;
    }
    
    // Get the next available slot
    uint32_t index = script_comp->count;
    
    // Slot the new script in
    script_comp->instances[index].instance_data = data;
    script_comp->instances[index].OnStart = start;
    script_comp->instances[index].OnUpdate = update;
    script_comp->instances[index].OnDestroy = destroy;
    script_comp->instances[index].has_started = false;
    
    // Increment the counter
    script_comp->count++;
    
    // Tell the ECS this entity has at least one script
    entity.scene->component_masks[entity.id] |= COMPONENT_SCRIPT;
}










Transform* Entity_GetTransform(Entity entity)
{
    if (!entity.scene) return NULL;
    
    // Ensure the entity actually HAS a transform before returning a pointer to it
    if ((entity.scene->component_masks[entity.id] & COMPONENT_TRANSFORM) == COMPONENT_TRANSFORM)
    {
        return &entity.scene->transforms[entity.id];
    }
    
    return NULL; // Component doesn't exist on this entity
}





RenderComponent* Entity_GetRenderable(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_RENDER) == COMPONENT_RENDER)
    {
        return &entity.scene->renderables[entity.id];
    }
    
    return NULL;
}





MeshHandle Entity_GetMesh(Entity entity)
{
    if (!Entity_IsValid(entity)) return (MeshHandle){0};
    
    // Check if the entity actually has a render component
    if (entity.scene->component_masks[entity.id] & COMPONENT_RENDER) {
        return (MeshHandle){ entity.scene->renderables[entity.id].mesh_id };
    }
    
    return (MeshHandle){0};
}





CameraComponent* Entity_GetCamera(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_CAMERA) == COMPONENT_CAMERA)
    {
        return &entity.scene->cameras[entity.id];
    }

    return NULL;
}





PointLightComponent* Entity_GetPointLight(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_POINT_LIGHT) == COMPONENT_POINT_LIGHT)
    {
        return &entity.scene->point_lights[entity.id];
    }

    return NULL;
}





ColliderComponent* Entity_GetCollider(Entity entity)
{
    if (!Entity_IsValid(entity)) return NULL;
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_COLLIDER)) return NULL;
    
    return &entity.scene->colliders[entity.id];
}





RigidbodyComponent* Entity_GetRigidbody(Entity entity)
{
    if (!Entity_IsValid(entity)) return NULL;
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_RIGIDBODY)) return NULL;
    
    return &entity.scene->rigidbodies[entity.id];
}















// Sets gravity for both engine and physics engine
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





void Rigidbody_SetKinematic(Entity entity, bool is_kinematic)
{
    RigidbodyComponent* rb = Entity_GetRigidbody(entity);
    ColliderComponent* col = Entity_GetCollider(entity);

    if (rb && col && col->physics_handle) 
    {
        rb->is_kinematic = is_kinematic;
        Physics_SetKinematicState(col->physics_handle, is_kinematic);
    }
}