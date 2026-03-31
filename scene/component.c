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