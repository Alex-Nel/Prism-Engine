#include "scene.h"



// Creates an entity with a name
Entity Entity_Create(Scene* scene, const char* name)
{
    if (!scene) return (Entity){0, NULL};

    // Finds the first available spot in the entity pool
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->component_masks[i] == COMPONENT_NONE)
        {
            Entity new_entity = {i, scene};

            if (name != NULL && name[0] != '\0')
                Entity_SetName(new_entity, name);
            else
                Entity_SetName(new_entity, "New Entity");
            

            // Adds the default transform
            Entity_AddTransform(
                new_entity,
                (Vector3){0.0f, 0.0f, 0.0f},
                QuaternionIdentity(),
                (Vector3){1.0f, 1.0f, 1.0f}
            );

            // Set the entity active in the scene
            scene->is_active_self[new_entity.id] = true;
            scene->is_active_in_hierarchy[new_entity.id] = true;

            return new_entity;
        }
    }
    
    // Scene is full, return NULL entity
    return (Entity){0, NULL}; 
}





// Adds an entity to the delete queue to be destroyed
void Entity_Destroy(Entity entity)
{
    if (!Entity_IsValid(entity)) return;

    Scene* scene = entity.scene;
    uint32_t id = entity.id;

    // Prevent double-queueing
    if (scene->is_pending_destroy[id])
        return;
    
    // Add to the queue
    scene->entities_to_destroy[scene->destroy_count++] = id;
    scene->is_pending_destroy[id] = true;

    Entity_SetActive(entity, false);

    // Recursively add all children
    uint32_t child_buffer[MAX_ENTITIES];
    uint32_t child_count = Entity_GetChildren(entity, child_buffer, MAX_ENTITIES, true);

    for (uint32_t i = 0; i < child_count; i++)
    {
        uint32_t child_id = child_buffer[i];

        if (!scene->is_pending_destroy[child_id])
        {
            scene->entities_to_destroy[scene->destroy_count++] = child_id;
            scene->is_pending_destroy[child_id] = true;
            Entity_SetActive((Entity){child_id, scene}, false);
        }
    }
}





// Returns if the entity is valid 
bool Entity_IsValid(Entity entity)
{
    // It is valid if the scene pointer is not null AND the mask is not NONE
    return (entity.scene != NULL) && (entity.scene->component_masks[entity.id] != COMPONENT_NONE);
}





// Sets the parent of one entity to a specified entity
void Entity_SetParent(Entity child, Entity parent)
{
    // Make sure both entities are in the scene and are not the same
    if (!Entity_IsValid(child) || !Entity_IsValid(parent)) return;
    if (child.id == parent.id) return;

    // If the parent is valid, remove the child from its previous parent
    if (!Entity_IsValid(parent))
    {
        Entity_RemoveParent(child);
        return;
    }

    Transform* child_t = &child.scene->transforms[child.id];
    Transform* parent_t = &parent.scene->transforms[parent.id];

    // Detach from old parent
    Entity_RemoveParent(child);

    // Attach to new parent
    child_t->parent_id = parent.id;
    child_t->next_sibling_id = ENTITY_NONE;
    child_t->prev_sibling_id = ENTITY_NONE;

    if (parent_t->first_child_id != ENTITY_NONE)
    {
        // If the parent already has children, find it's youngest sibling
        uint32_t current_sibling_id = parent_t->first_child_id;
        Transform* current_sibling = &child.scene->transforms[current_sibling_id];
        
        // Continue until one entity has no "next" sibling
        while (current_sibling->next_sibling_id != ENTITY_NONE)
        {
            current_sibling_id = current_sibling->next_sibling_id;
            current_sibling = &child.scene->transforms[current_sibling_id];
        }
        
        // Place child at the end of the line
        current_sibling->next_sibling_id = child.id;
        child_t->prev_sibling_id = current_sibling_id;
        
    }
    else
    {
        // The parent has no children.
        parent_t->first_child_id = child.id;
    }

    // Make sure to rebuild the matrix since the space just changed
    child_t->is_dirty = true; 
}





// Returns the parent of an entity
Entity Entity_GetParent(Entity entity)
{
    if (!Entity_IsValid(entity))
        return (Entity){ ENTITY_NONE, NULL };
    
    Transform* t = &entity.scene->transforms[entity.id];

    if (t->parent_id == ENTITY_NONE)
        return (Entity){ ENTITY_NONE, NULL };
    else
        return (Entity){ t->parent_id, entity.scene };
}





// Remove the parent from an entity
void Entity_RemoveParent(Entity child)
{
    if (!Entity_IsValid(child)) return;
    
    Transform* child_t = &child.scene->transforms[child.id];
    
    // If it already has no parent, return
    if (child_t->parent_id == ENTITY_NONE) return; 

    // Keep the entity at the same global position
    child_t->local_position = Transform_GetGlobalPosition(child_t);
    child_t->local_rotation = Transform_GetGlobalRotation(child_t);
    child_t->local_scale = Transform_GetGlobalScale(child_t);

    Transform* old_parent_t = &child.scene->transforms[child_t->parent_id];
    
    // Change the parents oldest child
    if (old_parent_t->first_child_id == child.id)
        old_parent_t->first_child_id = child_t->next_sibling_id;
    
    // Stitch the siblings together to close the gap
    if (child_t->prev_sibling_id != ENTITY_NONE)
        child.scene->transforms[child_t->prev_sibling_id].next_sibling_id = child_t->next_sibling_id;
    if (child_t->next_sibling_id != ENTITY_NONE)
        child.scene->transforms[child_t->next_sibling_id].prev_sibling_id = child_t->prev_sibling_id;

    // Remove parent and sibling IDs from the entity
    child_t->parent_id = ENTITY_NONE;
    child_t->next_sibling_id = ENTITY_NONE;
    child_t->prev_sibling_id = ENTITY_NONE;

    // Rebuild world matrix
    child_t->is_dirty = true;
}





// Set an entity to active or inactive
void Entity_SetActive(Entity entity, bool active)
{
    if (!Entity_IsValid(entity)) return;
    
    // Don't do anything if the new state is the same as the old
    if (entity.scene->is_active_self[entity.id] == active) return; 

    entity.scene->is_active_self[entity.id] = active;
    
    // Flag the transform as dirty to make the Scene Graph re-evaluate its transform + children
    Transform* t = Entity_GetTransform(entity);
    if (t) t->is_dirty = true;

    // Update the physics state if it has a collider
    if (entity.scene->component_masks[entity.id] & COMPONENT_COLLIDER)
    {
        ColliderComponent* c = &entity.scene->colliders[entity.id];
        if (c->physics_handle)
        {
            Physics_SetBodySimulationState(entity.scene->physics_world, c->physics_handle, active);
        }
    }
}





// Instantiates a 3D model as a hierarchy of child entities
void Entity_AddModel(Entity parent, Model* model)
{
    if (!Entity_IsValid(parent) || !model)
        return;


    // If the model is only a single mesh, just attach it to the parent
    if (model->node_count == 1)
    {
        Entity_AddRenderable(parent, model->nodes[0].mesh, model->nodes[0].material);
        return;
    }


    // For multi mesh models, spawn an entity for each mesh
    for (uint32_t i = 0; i < model->node_count; i++)
    {
        // Create the entity
        char child_name[MAX_NAME_LENGTH];
        snprintf(child_name, sizeof(child_name), "%s Node %d", model->name, i);
        Entity child = Entity_Create(parent.scene, child_name);

        // Set it up in the hierarchy
        Entity_SetParent(child, parent);

        // Make it's transform default
        Transform* child_t = Entity_GetTransform(child);
        Transform_SetLocalPosition(child_t, (Vector3){0, 0, 0});
        Transform_SetLocalRotation(child_t, (Quaternion){0, 0, 0, 1});
        Transform_SetLocalScale(child_t, (Vector3){1, 1, 1});

        // Give it the specific mesh and material
        Entity_AddRenderable(child, model->nodes[i].mesh, model->nodes[i].material);
    }
}





// Recursive function to walk the Left-Child/Right-Sibling tree
static void GatherDescendants(Scene* scene, uint32_t current_id, uint32_t* out_array, uint32_t max_count, uint32_t* current_count, bool recursive)
{
    if (current_id == ENTITY_NONE || *current_count >= max_count)
        return;

    // Start at the first child
    uint32_t child_id = scene->transforms[current_id].first_child_id;
    
    // Loop through all siblings
    while (child_id != ENTITY_NONE)
    {
        if (*current_count < max_count) 
        {
            out_array[*current_count] = child_id;
            (*current_count)++;

            // If recursive, dive into this child's children before moving to the next sibling
            if (recursive)
                GatherDescendants(scene, child_id, out_array, max_count, current_count, recursive);
        }

        child_id = scene->transforms[child_id].next_sibling_id;
    }
}





// Fills an array with child entity IDs. Returns the number of children found.
uint32_t Entity_GetChildren(Entity entity, uint32_t* out_array, uint32_t max_count, bool recursive)
{
    if (!Entity_IsValid(entity) || !out_array)
        return 0;
    
    uint32_t count = 0;
    GatherDescendants(entity.scene, entity.id, out_array, max_count, &count, recursive);

    return count;
}





// Walks up the tree hierarchy to find the first parent that has a specific component
Entity Entity_GetParentWithComponent(Entity entity, uint32_t component_mask)
{
    if (!Entity_IsValid(entity))
        return (Entity){ ENTITY_NONE, NULL };

    uint32_t current_parent = entity.scene->transforms[entity.id].parent_id;

    while (current_parent != ENTITY_NONE)
    {
        // Check if this parent has the component we want
        if ((entity.scene->component_masks[current_parent] & component_mask) == component_mask)
            return (Entity){ current_parent, entity.scene };
        
        // Move up to the next parent
        current_parent = entity.scene->transforms[current_parent].parent_id;
    }

    return (Entity){ ENTITY_NONE, NULL }; // Not found
}





// Remove physics properties from an entity
void Entity_RemovePhysics(Entity entity)
{
    if (!Entity_IsValid(entity)) return;

    // Check if the entity actually has physics attached
    if (entity.scene->component_masks[entity.id] & COMPONENT_COLLIDER)
    {
        ColliderComponent* c = &entity.scene->colliders[entity.id];
        
        if (c->physics_handle)
        {
            // Pass it to the bridge to permanently delete the C++ memory
            Physics_DestroyBody(entity.scene->physics_world, c->physics_handle);
            c->physics_handle = NULL;
        }
        
        // Strip the flags off the entity so the engine stops trying to sync it
        entity.scene->component_masks[entity.id] &= ~COMPONENT_COLLIDER;
        entity.scene->component_masks[entity.id] &= ~COMPONENT_RIGIDBODY;
    }
}





// Remove an entities rigidbody
void Entity_RemoveRigidbody(Entity entity)
{
    if (!Entity_IsValid(entity)) return;
    
    if (entity.scene->component_masks[entity.id] & COMPONENT_RIGIDBODY)
    {
        ColliderComponent* c = &entity.scene->colliders[entity.id];
        
        // Makes bullet turn it into a static mesh
        Physics_AddRigidbody(entity.scene->physics_world, c->physics_handle, 0.0f);
        
        // Remove the Rigidbody bit, but keep the Collider bit
        entity.scene->component_masks[entity.id] &= ~COMPONENT_RIGIDBODY;
    }
}










// Remove a specified component from an entity
void Entity_RemoveComponent(Entity entity, ComponentMask component)
{
    if (!Entity_IsValid(entity)) return;

    // Call respective functions depending on whether a rigidbody or collider is removed
    if (component & COMPONENT_COLLIDER)
    {
        Entity_RemovePhysics(entity); 
        
        component &= ~COMPONENT_COLLIDER;
        component &= ~COMPONENT_RIGIDBODY; 
    }
    else if (component & COMPONENT_RIGIDBODY)
    {
        Entity_RemoveRigidbody(entity);
        
        component &= ~COMPONENT_RIGIDBODY;
    }
    
    // The bitwise magic:
    // If the mask is 1111 (Has everything) and we want to remove 0010 (Transform):
    // ~0010 becomes 1101. 
    // 1111 AND 1101 = 1101 (Transform is now safely removed!)
    entity.scene->component_masks[entity.id] &= ~component;
}



// Removes a custom script
void Entity_UnbindScript(Entity entity, void* target_instance_data)
{
    if (!Entity_IsValid(entity)) return;
    
    // Check if the entity has any scripts
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_SCRIPT)) return;

    ScriptComponent* script_comp = &entity.scene->scripts[entity.id];

    // Search for the specific script we want to remove
    for (uint32_t i = 0; i < script_comp->count; i++)
    {
        if (script_comp->instances[i].instance_data == target_instance_data)
        {
            // If the script has an "OnDestroy function", call it
            if (script_comp->instances[i].OnDestroy != NULL)
            {
                // 1. The user provided custom cleanup! Run it!
                script_comp->instances[i].OnDestroy(entity, target_instance_data);
            }
            else
            {
                // No OnDestroy function provided, free the script memory
                free(target_instance_data); 
            }

            // Overwrite this slot with the data from the LAST slot in the array
            script_comp->instances[i] = script_comp->instances[script_comp->count - 1];
            
            // Shrink the array size
            script_comp->count--;

            // If that was the very last script, turn off the script mask entirely
            if (script_comp->count == 0)
                entity.scene->component_masks[entity.id] &= ~COMPONENT_SCRIPT;

            return;
        }
    }
}















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
    r->is_active = true;
    r->mesh = mesh;
    r->material = material;

    entity.scene->component_masks[entity.id] |= COMPONENT_RENDER;
}





// Adds a camera component to an entity with a specified FOX, near clipping plane, and far clipping plane
void Entity_AddCamera(Entity entity, float fov, float nearZ, float farZ)
{
    if (!entity.scene) return;

    CameraComponent* cam = &entity.scene->cameras[entity.id];
    cam->is_active = true;
    cam->fov = fov;
    cam->nearZ = nearZ;
    cam->farZ = farZ;
    cam->is_dirty = true;

    entity.scene->component_masks[entity.id] |= COMPONENT_CAMERA;
}





// Adds a point light component to an entity with specified light attributes
void Entity_AddLight(Entity entity, LightType type, Color color)
{
    if (!entity.scene) return;
    
    LightComponent* light = &entity.scene->lights[entity.id];
    light->is_active = true;
    light->type = type;
    light->color = color;
    light->intensity = 1.0f;
    light->ambient_strength = (type == LIGHT_DIRECTIONAL) ? 0.1f : 0.0f;
    light->constant = 1.0f;
    light->linear = 0.09f;
    light->quadratic = 0.032f;
    light->inner_cut_off = 12.5f;
    light->outer_cut_off = 17.5f;
    
    entity.scene->component_masks[entity.id] |= COMPONENT_LIGHT;
}





// Adds a box collider to an entity
void Entity_AddColliderBox(Entity entity, Vector3 extents, bool is_trigger)
{
    if (!Entity_IsValid(entity)) return;

    ColliderComponent* c = &entity.scene->colliders[entity.id];
    c->is_active = true;
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
    c->is_active = true;
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
    c->is_active = true;
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
    rb->is_active = true;
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
    listener->is_active = true;

    entity.scene->component_masks[entity.id] |= COMPONENT_AUDIO_LISTENER;
}





// Adds an audio source to an entity
void Entity_AddAudioSource(Entity entity)
{
    if (!Entity_IsValid(entity)) return;
    
    AudioSourceComponent* src = &entity.scene->audio_sources[entity.id];
    src->is_active = true;
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
    script_comp->instances[index].is_active = true;              // Scripts start active by default
    script_comp->instances[index].is_enabled_internal = false;   // Engine hasn't formally "enabled" it yet
    script_comp->instances[index].has_started = false;
    
    // Increment the counter
    script_comp->count++;
    
    // Tell the ECS this entity has at least one script
    entity.scene->component_masks[entity.id] |= COMPONENT_SCRIPT;
}





// Sets a scripts active state
void Script_SetActive(Entity entity, void* instance_data, bool active)
{
    if (!entity.scene) return;

    if (!(entity.scene->component_masks[entity.id] & COMPONENT_SCRIPT)) return;

    ScriptComponent* script_comp = &entity.scene->scripts[entity.id];
    
    // Find the specific instance
    for (uint32_t s = 0; s < script_comp->count; s++)
    {
        if (script_comp->instances[s].instance_data == instance_data)
        {
            script_comp->instances[s].is_active = active;
            return;
        }
    }
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
LightComponent* Entity_GetLight(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_LIGHT) == COMPONENT_LIGHT)
    {
        return &entity.scene->lights[entity.id];
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