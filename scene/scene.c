#include <string.h>
#include <stddef.h>
#include "scene.h"



// Creates a scene and returns a pointer
Scene* Scene_Create()
{
    // Allocate the struct on the Heap
    Scene* scene = (Scene*)malloc(sizeof(Scene));
    
    if (scene)
        Scene_Init(scene);
    else
        Log_Error("Failed to allocate memory for Scene!");
    
    return scene;
}





// Destroys a scene, removes everything
void Scene_Destroy(Scene* scene)
{
    if (!scene) return;

    // Shutdown physics, free any internal allocations
    Scene_ShutdownPhysics(scene); 
    
    // Free the struct
    free(scene);
}





// Initializes empty scene with default global light and physics
void Scene_Init(Scene* scene)
{
    if (!scene) return;


    // Mark all slots as empty
    memset(scene, COMPONENT_NONE, sizeof(Scene));

    scene->main_camera_id = 0;

    scene->physics_world = Physics_InitWorld();

    // Setting skybox information
    scene->skybox.background_color = (Color){0.8f, 0.8f, 0.8f, 1.0f};
    scene->skybox.texture = NULL;
    scene->skybox.shader = NULL;
    scene->has_skybox = false;
}





// Clears a scene
void Scene_Clear(Scene* scene)
{
    if (!scene) return;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        Entity e = { i, scene };

        // Dismantle physics components
        if (scene->component_masks[i] & COMPONENT_COLLIDER)
            Entity_RemovePhysics(e); 


        // Free any custom script memory
        if (scene->component_masks[i] & COMPONENT_SCRIPT)
        {
            ScriptComponent* sc = &scene->scripts[i];

            for (uint32_t s = 0; s < sc->count; s++)
            {
                if (sc->instances[s].OnDestroy)
                {
                    sc->instances[s].OnDestroy(e, sc->instances[s].instance_data);
                }
            }

            sc->count = 0;
        }

        // Remove all ECS data
        scene->component_masks[i] = 0;
        scene->is_active_in_hierarchy[i] = false;
        
        if (scene->names)
        {
            scene->names[i].name[0] = '\0';
        }
    }
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
// Runs custom scripts (OnUpdate) and audio sources/listeners
void Scene_Update(Scene* scene)
{
    if (!scene) return;

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

                // Catch state changes first
                if (script->is_active && !script->is_enabled_internal)
                {
                    if (script->OnEnable != NULL)
                        script->OnEnable(e, script->instance_data);
                    
                    script->is_enabled_internal = true;
                }
                else if (!script->is_active && script->is_enabled_internal)
                {
                    if (script->OnDisable != NULL)
                        script->OnDisable(e, script->instance_data);
                    
                    script->is_enabled_internal = false;
                }

                // Skip script if it's disabled
                if (!script->is_active)
                    continue;

                // Run OnStart once
                if (!script->has_started)
                {
                    if (script->OnStart != NULL)
                        script->OnStart(e, script->instance_data);

                    script->has_started = true;
                }

                // Run OnUpdate every frame
                if (script->OnUpdate != NULL)
                    script->OnUpdate(e, script->instance_data);
            }
        }
    }


    // Update all transforms in the scene
    Scene_UpdateTransforms(scene);



    // Update the main audio listener
    uint32_t listener_mask = COMPONENT_TRANSFORM | COMPONENT_AUDIO_LISTENER;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if ((scene->component_masks[i] & listener_mask) == listener_mask)
        {
            Transform* t = &scene->transforms[i];
            
            // Sync the listener to this entity's transform
            Vector3 pos = Transform_GetGlobalPosition(t);
            Vector3 forward = Transform_GetForwardVector(t);
            Vector3 up = Transform_GetUpVector(t);
            
            Audio_SetListenerPosition(pos, forward, up);
            break; // Only 1 listener per scene, so we break
        }
    }


    // Update all audio sources
    uint32_t source_mask = COMPONENT_TRANSFORM | COMPONENT_AUDIO_SOURCE;
    
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if ((scene->component_masks[i] & source_mask) == source_mask)
        {
            Transform* t = &scene->transforms[i];
            AudioSourceComponent* src = &scene->audio_sources[i];

            // Setup spatial parameters
            Audio_SetSpatial(src->clip, src->is_spatial);
            if (src->is_spatial)
            {
                Vector3 pos = Transform_GetGlobalPosition(t);
                Audio_SetSourcePosition(src->clip, pos);
                Audio_SetSourceDistances(src->clip, src->min_distance, src->max_distance);
            }

            if (src->is_playing)
            {
                if (!Audio_IsPlaying(src->clip))
                    Audio_Play(src->clip, src->volume, src->loop);
            }
            else
            {
                if (Audio_IsPlaying(src->clip))
                    Audio_Stop(src->clip);
            }
        }
    }
}





// Runs physics and other fixed time operations
void Scene_FixedUpdate(Scene* scene)
{
    if (!scene) return;

    // Execute Fixed Updates in scripts
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        if (scene->component_masks[i] & COMPONENT_SCRIPT)
        {
            ScriptComponent* script_comp = &scene->scripts[i];
            Entity e = { i, scene };

            for (uint32_t s = 0; s < script_comp->count; s++)
            {
                ScriptInstance* script = &script_comp->instances[s];

                // Catch state changes first
                if (script->is_active && !script->is_enabled_internal)
                {
                    if (script->OnEnable != NULL)
                        script->OnEnable(e, script->instance_data);
                    
                    script->is_enabled_internal = true;
                }
                else if (!script->is_active && script->is_enabled_internal)
                {
                    if (script->OnDisable != NULL)
                        script->OnDisable(e, script->instance_data);
                    
                    script->is_enabled_internal = false;
                }

                // Skip script if it's disabled
                if (!script->is_active)
                    continue;
                
                // Ensure OnStart has run before FixedUpdate
                if (!script->has_started)
                {
                    if (script->OnStart != NULL)
                        script->OnStart(e, script->instance_data);
                    
                    script->has_started = true;
                }

                if (script->OnFixedUpdate != NULL)
                    script->OnFixedUpdate(e, script->instance_data);
            }
        }
    }


    // update transforms incase scripts moved objects
    Scene_UpdateTransforms(scene);


    // Pre-Physics Sync: Push Manual Transforms To Physics
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;
        if (!(scene->component_masks[i] & COMPONENT_TRANSFORM)) continue;

        Transform* t = &scene->transforms[i];

        bool has_collider = (scene->component_masks[i] & COMPONENT_COLLIDER);
        bool has_rigidbody = (scene->component_masks[i] & COMPONENT_RIGIDBODY);
        
        bool is_static = has_collider && !has_rigidbody;
        bool is_kinematic = has_rigidbody && scene->rigidbodies[i].is_kinematic;
        bool is_dynamic = has_rigidbody && !scene->rigidbodies[i].is_kinematic;

        if (is_static || is_kinematic || (is_dynamic && t->is_dirty))
        {
            ColliderComponent* c = &scene->colliders[i];
            if (c->physics_handle)
            {
                Vector3 global_pos = Transform_GetGlobalPosition(t);
                Quaternion global_rot = Transform_GetGlobalRotation(t);
                Vector3 global_scale = Transform_GetGlobalScale(t);

                Physics_SetBodyPosition(c->physics_handle, global_pos);
                Physics_SetBodyRotation(c->physics_handle, global_rot);
                Physics_SetBodyScale(c->physics_handle, global_scale);

                if (is_dynamic)
                    Physics_SetLinearVelocity(c->physics_handle, (Vector3){0,0,0});
            }
        }
    }


    // Step Physics Simulation
    if (scene->physics_world)
    {
        // Important to use the FIXED delta time here, not the variable one
        float fixed_dt = Time_FixedDeltaTime();
        Physics_StepSimulation(scene->physics_world, fixed_dt);

        // Shift current memory to "last frame" memory
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (scene->component_masks[i] & COMPONENT_COLLIDER)
            {
                ColliderComponent* c = &scene->colliders[i];
                memcpy(c->touching_last_frame, c->touching_entities, sizeof(uint32_t) * c->touching_count);
                c->touching_last_count = c->touching_count;
                c->touching_count = 0; 
            }
        }


        // Fetch Collisions for all colliders
        CollisionPair pairs[1024]; 
        int hit_count = Physics_GetCollisions(scene->physics_world, pairs, 1024);

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


        // Process Collision Events
        for (uint32_t i = 0; i < MAX_ENTITIES; i++)
        {
            if (!(scene->component_masks[i] & COMPONENT_COLLIDER)) continue;
            
            ColliderComponent* c = &scene->colliders[i];
            
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


    // Post-Physics Sync: Pull dynamic transforms from Physics
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;
        
        // If it has a Rigidbody, physics engine controls positions/rotations
        if ((scene->component_masks[i] & COMPONENT_RIGIDBODY) && !scene->rigidbodies[i].is_kinematic)
        {
            ColliderComponent* c = &scene->colliders[i];

            if (c->physics_handle)
            {
                Transform* t = &scene->transforms[i];

                Vector3 phys_global_pos = Physics_GetBodyPosition(c->physics_handle);
                Quaternion phys_global_rot = Physics_GetBodyRotation(c->physics_handle);

                if (t->parent_id == ENTITY_NONE)
                {
                    // If an entity has no parent, the world space is the local space
                    Transform_SetLocalPosition(t, phys_global_pos);
                    Transform_SetLocalRotation(t, phys_global_rot);
                }
                else
                {
                    // If an entity has a parent, convert global physics pos/rot to local space
                    Transform* parent_t = &scene->transforms[t->parent_id];

                    // Local position = Inverse(parent world matrix) * Global Position
                    Matrix4 inv_parent_matrix = Matrix4Inverse(parent_t->world_matrix);
                    Vector3 local_pos = Matrix4MultiplyVector3(inv_parent_matrix, phys_global_pos);

                    // Local rotation = Inverse(Parent Global Rotation) * Global Rotation
                    Quaternion parent_global_rot = Transform_GetGlobalRotation(parent_t);
                    Quaternion inv_parent_rot = QuaternionInverse(parent_global_rot);
                    Quaternion local_rot = QuaternionMultiply(inv_parent_rot, phys_global_rot);

                    Transform_SetLocalPosition(t, local_pos);
                    Transform_SetLocalRotation(t, local_rot);
                }
            }
        }
    }

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





// Returns the total number of entities that exist in a scene
uint32_t Scene_GetTotalEntityCount(Scene* scene)
{
    if (!scene)
        return 0;

    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        // If the mask is greater than 0, then the entity slot is in use
        if (scene->component_masks[i] > 0)
            count++;
    }

    return count;
}





// Returns only the number of active entities in a scene
uint32_t Scene_GetActiveEntityCount(Scene* scene)
{
    if (!scene)
        return 0;

    uint32_t count = 0;

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        // Note: check is_active_in_hierarchy, not is_active_self.
        // An entity might be active, but have a disabled parent, which means it's also disabled
        if (scene->component_masks[i] > 0 && scene->is_active_in_hierarchy[i])
            count++;
    }

    return count;
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





// Deletes all entities in the destroy queue
void Scene_ProcessDestroyQueue(Scene* scene)
{
    if (!scene || scene->destroy_count == 0)
        return;
    
    // Loop through all entities to destroy
    for (uint32_t i = 0; i < scene->destroy_count; i++)
    {
        uint32_t id = scene->entities_to_destroy[i];

        // Run OnDestroy() before wiping memory
        if (scene->component_masks[id] & COMPONENT_SCRIPT)
        {
            ScriptComponent* script_comp = &scene->scripts[id];
            Entity e = { id, scene };

            for (uint32_t s = 0; s < script_comp->count; s++)
            {
                ScriptInstance* script = &script_comp->instances[s];

                if (script->has_started && script->OnDestroy != NULL)
                    script->OnDestroy(e, script->instance_data);
            }
        }

        // Cleanup physics (remove rigidbodies and colliders)
        if (scene->component_masks[id] & COMPONENT_COLLIDER)
        {
            ColliderComponent* c = &scene->colliders[id];

            if (c->physics_handle)
            {
                Physics_DestroyBody(scene->physics_world, c->physics_handle);
                c->physics_handle = NULL;
            }
        }

        // Unparent it from the hierarchy
        Entity_RemoveParent((Entity){id, scene});
        
        // Clear all masks
        scene->component_masks[id] = 0;
        scene->is_active_self[id] = false;
        scene->is_active_in_hierarchy[id] = false;
        scene->is_pending_destroy[id] = false;

        scene->names[id].name[0] = '\0';
    }

    // Reset queue for next frame
    scene->destroy_count = 0;
}





// Sets the skybox of the scene
void Scene_SetSkybox(Scene* scene, Texture* skybox_texture, Shader* skybox_shader)
{
    if (!scene)
        return;
    
    if (!scene->has_skybox)
        scene->has_skybox = true;

    scene->skybox.texture = skybox_texture;
    scene->skybox.shader = skybox_shader;
}





// Removes the skybox from the scene
void Scene_RemoveSkybox(Scene* scene)
{
    if (!scene)
        return;
    
    scene->has_skybox = false;
}





// Performs a raycast from an origin, direction, and distance
bool Scene_Raycast(Scene* scene, Ray ray, float max_distance, RaycastHit* out_hit, int collision_mask, bool hit_triggers)
{
    if (!scene || !scene->physics_world)
        return false;

    return Physics_Raycast(scene->physics_world, ray, max_distance, out_hit, collision_mask, hit_triggers);
}





// Performs a raycast from origin to distance. Returns all hit objects up to max_hits
int Scene_RaycastAll(Scene* scene, Ray ray, float max_distance, RaycastHit* out_hits, int max_hits, int collision_mask, bool hit_triggers)
{
    if (!scene || !scene->physics_world)
        return 0;

    return Physics_RaycastAll(scene->physics_world, ray, max_distance, out_hits, max_hits, collision_mask, hit_triggers);
}