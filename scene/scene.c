#include "scene/scene.h"
#include <string.h> // For memset
#include <stddef.h> // For NULL

// --- SCENE SYSTEM ---

void Scene_Init(Scene* scene)
{
    if (!scene) return;

    // scene = malloc(sizeof(Scene));

    // Mark all slots as empty
    // memset(scene->component_masks, COMPONENT_NONE, sizeof(scene->component_masks));
    memset(scene, COMPONENT_NONE, sizeof(Scene));

    scene->main_camera_id = 0;

    scene->global_light.direction = (Vector3){1.0f, 2.0f, 1.0f};
    scene->global_light.color = (Vector3){1.0f, 1.0f, 1.0f}; // Pure white
    scene->global_light.ambient_strength = 0.2f;
}

void Scene_Update(Scene* scene)
{
    if (!scene) return;

    float dt = Time_DeltaTime();

    // 1. EXECUTE SCRIPTS
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->component_masks[i] & COMPONENT_SCRIPT)
        {
            ScriptComponent* script_comp = &scene->scripts[i];
            Entity e = { i, scene };

            // --- Loop through every script attached to THIS entity ---
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


    // Loop through all entities to update their math
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if ((scene->component_masks[i] & COMPONENT_TRANSFORM) == COMPONENT_TRANSFORM)
        {
            Transform* t = &scene->transforms[i];
            
            // If the object moved, recalculate its matrix
            if (t->is_dirty)
            {
                t->world_matrix = Matrix4CreateTransform(t->position, t->rotation, t->scale);
                t->is_dirty = false;
            }
        }
    }
}

// --- ENTITY LIFECYCLE ---

Entity Entity_Create(Scene* scene, const char* name)
{
    if (!scene) return (Entity){0, NULL};

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (scene->component_masks[i] == COMPONENT_NONE)
        {
            Entity new_entity = {i, scene};

            if (name != NULL && name[0] != '\0')
                Entity_SetName(new_entity, name);
            else
                Entity_SetName(new_entity, "New Entity");
            

            Entity_AddTransform(
                new_entity,
                (Vector3){0.0f, 0.0f, 0.0f},
                QuaternionIdentity(),
                (Vector3){1.0f, 1.0f, 1.0f}
            );

            return new_entity;
        }
    }
    
    // Scene is full
    return (Entity){0, NULL}; 
}



void Entity_Destroy(Entity entity)
{
    if (Entity_IsValid(entity))
    {
        // Erasing the mask instantly "deletes" the entity and all its components
        entity.scene->component_masks[entity.id] = COMPONENT_NONE;
    }
}



bool Entity_IsValid(Entity entity)
{
    // It is valid if the scene pointer is not null AND the mask is not NONE
    return (entity.scene != NULL) && 
           (entity.scene->component_masks[entity.id] != COMPONENT_NONE);
}







// --- COMPONENT SETTERS ---

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
    t->position = position;
    t->rotation = rotation;
    t->scale = scale;
    t->is_dirty = true;

    t->rotation_euler = (Vector3){0.0f, 0.0f, 0.0f};

    // Register the component
    entity.scene->component_masks[entity.id] |= COMPONENT_TRANSFORM;
}



void Entity_SetRotationEuler(Entity entity, Vector3 euler_angles)
{
    if (!entity.scene) return;
    
    Transform* t = &entity.scene->transforms[entity.id];
    
    // 1. Update the human-readable angles
    t->rotation_euler = euler_angles;
    
    // 2. Automatically bake the computer-readable Quaternion
    t->rotation = QuaternionFromEuler(euler_angles.x, euler_angles.y, euler_angles.z);
    
    // 3. Automatically flag for a matrix rebuild
    t->is_dirty = true;
}



void Entity_SetPosition(Entity entity, Vector3 position)
{
    if (!entity.scene) return;
    Transform* t = &entity.scene->transforms[entity.id];
    t->position = position;
    t->is_dirty = true;
}



void Entity_AddRenderable(Entity entity, uint32_t mesh_id, uint32_t shader_id, uint32_t texture_id)
{
    if (!entity.scene) return;

    if (texture_id == 0)
        texture_id = Asset_GetDefaultTexture().id;

    RenderComponent* r = &entity.scene->renderables[entity.id];
    r->mesh_id = mesh_id;
    r->shader_id = shader_id;
    r->texture_id = texture_id;

    // Register the component
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







// --- COMPONENT GETTERS ---

Entity Scene_GetEntity(Scene* scene, const char* name)
{
    if (!scene || !name) return (Entity){ 0, NULL };

    // Search every active entity in the scene
    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        // Only check entities that actually have a name component
        if (scene->component_masks[i] & COMPONENT_NAME)
        {       
            // If the strings match, we found our entity!
            if (strcmp(scene->names[i].name, name) == 0)
            {
                return (Entity){ i, scene };
            }
        }
    }

    // If we didn't find it, return a null entity
    return (Entity){ 0, NULL }; 
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



CameraComponent* Entity_GetCamera(Entity entity)
{
    if (!entity.scene) return NULL;

    if ((entity.scene->component_masks[entity.id] & COMPONENT_CAMERA) == COMPONENT_CAMERA)
    {
        return &entity.scene->cameras[entity.id];
    }
    return NULL;
}







// Remove components
void Entity_RemoveComponent(Entity entity, ComponentMask component)
{
    if (!entity.scene) return;
    
    // The bitwise magic:
    // If the mask is 1111 (Has everything) and we want to remove 0010 (Transform):
    // ~0010 becomes 1101. 
    // 1111 AND 1101 = 1101 (Transform is now safely removed!)
    entity.scene->component_masks[entity.id] &= ~component;
}



// Remove custom scripts
void Entity_UnbindScript(Entity entity, void* target_instance_data)
{
    if (!entity.scene) return;
    
    // Check if the entity even has scripts
    if (!(entity.scene->component_masks[entity.id] & COMPONENT_SCRIPT)) return;

    ScriptComponent* script_comp = &entity.scene->scripts[entity.id];

    // Search for the specific script we want to remove
    for (uint32_t i = 0; i < script_comp->count; i++)
    {    
        // We identify the script by checking its unique memory pointer
        if (script_comp->instances[i].instance_data == target_instance_data)
        {
            // --- THE FALLBACK CLEANUP LOGIC ---
            if (script_comp->instances[i].OnDestroy != NULL)
            {
                // 1. The user provided custom cleanup! Run it!
                script_comp->instances[i].OnDestroy(entity, target_instance_data);
            }
            else
            {
                // 2. The user was lazy (passed NULL). We do a default free
                free(target_instance_data); 
            }


            // --- SWAP AND POP ---
            // 1. Overwrite this slot with the data from the LAST slot in the array
            script_comp->instances[i] = script_comp->instances[script_comp->count - 1];
            
            // 2. Shrink the array size
            script_comp->count--;

            // 3. If that was the very last script, turn off the script mask entirely!
            if (script_comp->count == 0)
            {
                entity.scene->component_masks[entity.id] &= ~COMPONENT_SCRIPT;
            }

            return;
        }
    }
}







// Other functions

void Scene_SetMainCamera(Scene* scene, Entity camera_entity)
{
    if (scene && Entity_IsValid(camera_entity))
    {
        scene->main_camera_id = camera_entity.id;
    }
}



void Entity_Translate(Entity entity, Vector3 translation)
{
    Transform* t = Entity_GetTransform(entity);
    t->position.x += translation.x;
    t->position.y += translation.y;
    t->position.z += translation.z;
}



void Entity_RotateEuler(Entity entity, Vector3 euler_addition)
{
    if (!entity.scene) return;
    Transform* t = &entity.scene->transforms[entity.id];
    
    Vector3 new_euler = {
        t->rotation_euler.x + euler_addition.x,
        t->rotation_euler.y + euler_addition.y,
        t->rotation_euler.z + euler_addition.z
    };
    
    // Re-use the setter!
    Entity_SetRotationEuler(entity, new_euler);
}