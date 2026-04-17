#include <string.h>
#include <stddef.h>
#include "scene.h"



// Initializes empty scene with default global light and physics
void Scene_Init(Scene* scene)
{
    if (!scene) return;


    // Mark all slots as empty
    memset(scene, COMPONENT_NONE, sizeof(Scene));

    scene->main_camera_id = 0;

    scene->global_light.direction = (Vector3){1.0f, 2.0f, 1.0f};
    scene->global_light.color = (Vector3){1.0f, 1.0f, 1.0f}; // Pure white
    scene->global_light.ambient_strength = 0.2f;

    scene->physics_world = Physics_InitWorld();
}





// Simple array search
static bool ArrayContains(uint32_t* array, uint32_t count, uint32_t value)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (array[i] == value) return true;
    }

    return false;
}





// Event dispatcher enum
typedef enum { EVENT_ENTER, EVENT_STAY, EVENT_EXIT } CollisionEventType;





// Dispatches collision events to any scripts
static void DispatchCollisionEvent(Scene* scene, uint32_t entity_id, uint32_t other_id, CollisionEventType event_type)
{
    // If this entity has no scripts, do nothing
    if (!(scene->component_masks[entity_id] & COMPONENT_SCRIPT)) return;
    
    ScriptComponent* sc = &scene->scripts[entity_id];
    ColliderComponent* c1 = &scene->colliders[entity_id];
    ColliderComponent* c2 = &scene->colliders[other_id];
    
    // If at least one collider is a trigger, it is a Trigger Event. Otherwise, it's a Collision Event.
    bool is_trigger = (c1->is_trigger || c2->is_trigger);
    
    Entity self = { entity_id, scene };
    Entity other = { other_id, scene };
    
    for (uint32_t s = 0; s < sc->count; s++)
    {
        ScriptInstance* script = &sc->instances[s];
        
        if (is_trigger) 
        {
            if (event_type == EVENT_ENTER && script->OnTriggerEnter) script->OnTriggerEnter(self, other, script->instance_data);
            if (event_type == EVENT_STAY  && script->OnTriggerStay)  script->OnTriggerStay(self, other, script->instance_data);
            if (event_type == EVENT_EXIT  && script->OnTriggerExit)  script->OnTriggerExit(self, other, script->instance_data);
        } 
        else 
        {
            if (event_type == EVENT_ENTER && script->OnCollisionEnter) script->OnCollisionEnter(self, other, script->instance_data);
            if (event_type == EVENT_STAY  && script->OnCollisionStay)  script->OnCollisionStay(self, other, script->instance_data);
            if (event_type == EVENT_EXIT  && script->OnCollisionExit)  script->OnCollisionExit(self, other, script->instance_data);
        }
    }
}





// Updates the scene
// Runs custom scripts and runs the physics engine for a frame
void Scene_Update(Scene* scene)
{
    if (!scene) return;

    float dt = Time_DeltaTime();

    // Execute any custom scripts
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if (scene->component_masks[i] & COMPONENT_SCRIPT)
        {
            ScriptComponent* script_comp = &scene->scripts[i];
            Entity e = { i, scene };

            // Loop through every script attached to this entity
            for (uint32_t s = 0; s < script_comp->count; s++)
            {
                ScriptInstance* script = &script_comp->instances[s];

                // Run OnStart once
                if (!script->has_started)
                {
                    if (script->OnStart != NULL)
                    {
                        script->OnStart(e, script->instance_data);
                    }

                    script->has_started = true;
                }

                // Run OnUpdate every frame
                if (script->OnUpdate != NULL)
                {
                    script->OnUpdate(e, dt, script->instance_data);
                }
            }
        }
    }


    // Sync engine positions/rotations with physics engine
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        bool is_static_collider = (scene->component_masks[i] & COMPONENT_COLLIDER) && !(scene->component_masks[i] & COMPONENT_RIGIDBODY);
        bool is_kinematic_body = (scene->component_masks[i] & COMPONENT_RIGIDBODY) && scene->rigidbodies[i].is_kinematic;
        
        if (is_static_collider || is_kinematic_body)
        {
            ColliderComponent* c = &scene->colliders[i];
            if (c->physics_handle)
            {
                Transform* t = &scene->transforms[i];
                Physics_SetBodyPosition(c->physics_handle, t->local_position);
                Physics_SetBodyRotation(c->physics_handle, t->local_rotation);
            }
        }
    }


    // Step through physics simulation
    if (scene->physics_world)
    {
        Physics_StepSimulation(scene->physics_world, dt);

        // Shift current memory to "last frame" memory
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (scene->component_masks[i] & COMPONENT_COLLIDER)
            {
                ColliderComponent* c = &scene->colliders[i];
                memcpy(c->touching_last_frame, c->touching_entities, sizeof(uint32_t) * c->touching_count);
                c->touching_last_count = c->touching_count;
                c->touching_count = 0; // Reset this frame's counter
            }
        }

        // Fetch all overlapping pairs from Bullet
        CollisionPair pairs[1024]; 
        int hit_count = Physics_GetCollisions(scene->physics_world, pairs, 1024);

        // Process all pairs
        for (int i = 0; i < hit_count; i++)
        {
            uint32_t idA = pairs[i].entity_a;
            uint32_t idB = pairs[i].entity_b;

            // Save the hit to Entity A's memory
            ColliderComponent* colA = &scene->colliders[idA];
            if (colA->touching_count < MAX_COLLISION_OVERLAPS)
                colA->touching_entities[colA->touching_count++] = idB;

            // Save the hit to Entity B's memory
            ColliderComponent* colB = &scene->colliders[idB];
            if (colB->touching_count < MAX_COLLISION_OVERLAPS)
                colB->touching_entities[colB->touching_count++] = idA;
        }


        // Process all collision callbacks
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!(scene->component_masks[i] & COMPONENT_COLLIDER)) continue;
            
            ColliderComponent* c = &scene->colliders[i];
            
            // Check for ENTER and STAY events
            for (uint32_t j = 0; j < c->touching_count; j++)
            {
                uint32_t other_id = c->touching_entities[j];
                
                // If it was touching last frame, it's an EVENT_STAY, otherwise it's an EVENT_ENTER
                bool was_touching = ArrayContains(c->touching_last_frame, c->touching_last_count, other_id);
                
                if (was_touching)
                    DispatchCollisionEvent(scene, i, other_id, EVENT_STAY);
                else
                    DispatchCollisionEvent(scene, i, other_id, EVENT_ENTER);
            }
            
            // Check for EXIT events
            for (uint32_t j = 0; j < c->touching_last_count; j++)
            {
                uint32_t other_id = c->touching_last_frame[j];
                
                // If it's not touching this frame, it's an EVENT_EXIT
                bool is_touching_now = ArrayContains(c->touching_entities, c->touching_count, other_id);
                
                if (!is_touching_now)
                    DispatchCollisionEvent(scene, i, other_id, EVENT_EXIT);
            }
        }
    }


    // Sync physics positions/rotations with the engine
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;
        
        // If it has a Rigidbody, physics engine controls positions/rotations
        if ((scene->component_masks[i] & COMPONENT_RIGIDBODY) && !scene->rigidbodies[i].is_kinematic)
        {
            ColliderComponent* c = &scene->colliders[i];
            if (c->physics_handle) {
                Transform* t = &scene->transforms[i];
                Vector3 new_pos = Physics_GetBodyPosition(c->physics_handle);
                Quaternion new_rot = Physics_GetBodyRotation(c->physics_handle);
                
                Transform_SetLocalPosition(t, new_pos);
                Transform_SetLocalRotation(t, new_rot);
            }
        }
    }

    // Update all transforms in the scene
    Scene_UpdateTransforms(scene);
}





// Helper function to trigger enable/disable events
static void TriggerScriptStateChange(Scene* scene, uint32_t entity_id, bool is_enabling)
{
    if (!(scene->component_masks[entity_id] & COMPONENT_SCRIPT)) return;

    ScriptComponent* script_comp = &scene->scripts[entity_id];
    Entity e = { entity_id, scene };

    for (uint32_t s = 0; s < script_comp->count; s++)
    {
        ScriptInstance* script = &script_comp->instances[s];
        
        if (is_enabling && script->OnEnable != NULL)
            script->OnEnable(e, script->instance_data);
        else if (!is_enabling && script->OnDisable != NULL)
            script->OnDisable(e, script->instance_data);
    }
}





// Helper function to update the scene tree
static void UpdateTransformTree(Scene* scene, uint32_t entity_id, Matrix4* parent_world, bool force_update)
{
    if (entity_id == ENTITY_NONE) return;

    Transform* t = &scene->transforms[entity_id];

    // Hierarchy cascade for enabling and disabling
    bool was_active = scene->is_active_in_hierarchy[entity_id];
    
    bool parent_is_active = (t->parent_id != ENTITY_NONE) ? scene->is_active_in_hierarchy[t->parent_id] : true;
    
    bool now_active = scene->is_active_self[entity_id] && parent_is_active;
    scene->is_active_in_hierarchy[entity_id] = now_active;

    if (was_active != now_active)
    {
        TriggerScriptStateChange(scene, entity_id, now_active);
        force_update = true;
    }
    

    // If entity or parent moved, recalculate
    bool needs_update = t->is_dirty || force_update;

    if (needs_update)
    {
        Matrix4 local = Matrix4CreateTransform(t->local_position, t->local_rotation, t->local_scale);

        // Build the World Matrix
        if (parent_world != NULL)
            t->world_matrix = Matrix4Multiply(*parent_world, local);
        else
            t->world_matrix = local;

        // transform is clean for this frame
        t->is_dirty = false;
    }


    // --- LEFT-CHILD, RIGHT-SIBLING TRAVERSAL ---
    // Process the oldest child. Pass the parents world_matrix to them
    if (t->first_child_id != ENTITY_NONE)
        UpdateTransformTree(scene, t->first_child_id, &t->world_matrix, needs_update);

    // Process our next sibling. Siblings share the same parent, 
    if (t->next_sibling_id != ENTITY_NONE)
        UpdateTransformTree(scene, t->next_sibling_id, parent_world, force_update);
}





// Updates all transforms in a scene graph
void Scene_UpdateTransforms(Scene* scene)
{
    if (!scene) return;

    uint32_t mask = COMPONENT_TRANSFORM;

    // Find all the Root entities and build their trees downwards
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if ((scene->component_masks[i] & mask) == mask)
        {
            if (scene->transforms[i].parent_id == ENTITY_NONE)
            {
                // Start recursion
                UpdateTransformTree(scene, i, NULL, false);
            }
        }
    }
}





// Returns the entity from searching by name (slow)
Entity Scene_GetEntity(Scene* scene, const char* name)
{
    if (!scene || !name) return (Entity){ 0, NULL };

    // Search every active entity in the scene
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        // Only check entities that actually have a name component (all entities should, but just in case)
        if (scene->component_masks[i] & COMPONENT_NAME)
        {       
            // If the strings match, return entity
            if (strcmp(scene->names[i].name, name) == 0)
            {
                return (Entity){ i, scene };
            }
        }
    }

    // If no match, return NULL entity
    return (Entity){ 0, NULL }; 
}





// Sets the main camera of a scene
void Scene_SetMainCamera(Scene* scene, Entity camera_entity)
{
    if (scene && Entity_IsValid(camera_entity))
    {
        scene->main_camera_id = camera_entity.id;
    }
}





// Shuts down the physics engine of the scene
void Scene_ShutdownPhysics(Scene* scene)
{
    if (!scene || !scene->physics_world) return;

    // Destroy all rigidbodies before destroying the world
    // Bullet will crash if world gets deleted with rigidbodies still active
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->component_masks[i] & COMPONENT_COLLIDER)
        {
            Entity e = { i, scene };
            Entity_RemovePhysics(e);
        }
    }

    Physics_ShutdownWorld(scene->physics_world);
    scene->physics_world = NULL;
}





// Performs a raycast from an origin, direction, and distance
bool Scene_Raycast(Scene* scene, Vector3 origin, Vector3 direction, float max_distance, RaycastHit* out_hit, int collision_mask)
{
    if (!scene || !scene->physics_world) return false;

    // Normalize the direction vector
    float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    
    if (length > 0.0f)
    {
        direction.x /= length;
        direction.y /= length;
        direction.z /= length;
    }

    // Calculate the end point of the ray
    Vector3 end = {
        origin.x + (direction.x * max_distance),
        origin.y + (direction.y * max_distance),
        origin.z + (direction.z * max_distance)
    };

    // Pass it to Bullet
    return Physics_Raycast(scene->physics_world, origin, end, out_hit, collision_mask);
}





// Performs a raycast from origin to distance. Returns all hit objects up to max_hits
int Scene_RaycastAll(Scene* scene, Vector3 origin, Vector3 direction, float max_distance, RaycastHit* out_hits, int max_hits, int collision_mask)
{
    if (!scene || !scene->physics_world) return 0;

    // Normalize the direction vector
    float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    
    if (length > 0.0f)
    {
        direction.x /= length;
        direction.y /= length;
        direction.z /= length;
    }

    // Calculate the end point
    Vector3 end = {
        origin.x + (direction.x * max_distance),
        origin.y + (direction.y * max_distance),
        origin.z + (direction.z * max_distance)
    };

    return Physics_RaycastAll(scene->physics_world, origin, end, out_hits, max_hits, collision_mask);
}










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





// Deletes an entity from the scene
void Entity_Destroy(Entity entity)
{
    if (!Entity_IsValid(entity)) return;

    // Cleans the entity from the scene graph
    if (entity.scene->component_masks[entity.id] & COMPONENT_TRANSFORM) 
    {
        Transform* t = &entity.scene->transforms[entity.id];

        // Recursively delete the entities children
        uint32_t current_child_id = t->first_child_id;
        while (current_child_id != ENTITY_NONE) 
        {
            // Save the next sibling's ID before we destroy the current child!
            // If we destroy the child first, its data is wiped and we lose the sibling chain.
            uint32_t next_id = entity.scene->transforms[current_child_id].next_sibling_id;
            
            Entity child_entity = { current_child_id, entity.scene };
            Entity_Destroy(child_entity);
            
            current_child_id = next_id;
        }

        // Detach entity from its parent so we don't leave a dead pointer in their sibling list
        Entity_RemoveParent(entity);
    }

    // Erase entity from the ECS
    entity.scene->component_masks[entity.id] = COMPONENT_NONE;
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
    if (!child.scene || !parent.scene) return;
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





// Remove the parent from an entity
void Entity_RemoveParent(Entity child)
{
    if (!child.scene) return;
    
    Transform* child_t = &child.scene->transforms[child.id];
    
    // If it already has no parent, return
    if (child_t->parent_id == ENTITY_NONE) return; 

    // Keep the entity at the same global position
    child_t->local_position = Transform_GetGlobalPosition(child_t);
    // TODO: Set global rotation and scale too

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
    if (!entity.scene) return;
    
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
    if (!entity.scene) return;

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
    if (!entity.scene) return;
    
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
