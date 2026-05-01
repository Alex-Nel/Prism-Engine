#include "scene.h"
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



static cJSON* SaveVec3(Vector3 v)
{
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.x));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.y));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.z));
    return arr;
}

static cJSON* SaveQuat(Quaternion q)
{
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(q.x));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(q.y));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(q.z));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(q.w));
    return arr;
}

static Vector3 LoadVec3(cJSON* arr)
{
    if (!arr || cJSON_GetArraySize(arr) < 3)
        return (Vector3){0,0,0};

    return (Vector3){
        cJSON_GetArrayItem(arr, 0)->valuedouble,
        cJSON_GetArrayItem(arr, 1)->valuedouble,
        cJSON_GetArrayItem(arr, 2)->valuedouble
    };
}

static Quaternion LoadQuat(cJSON* arr)
{
    if (!arr || cJSON_GetArraySize(arr) < 4)
        return (Quaternion){0,0,0,1};
    
        return (Quaternion){
        cJSON_GetArrayItem(arr, 0)->valuedouble,
        cJSON_GetArrayItem(arr, 1)->valuedouble,
        cJSON_GetArrayItem(arr, 2)->valuedouble,
        cJSON_GetArrayItem(arr, 3)->valuedouble
    };
}





// Saves the current state of the scene to a JSON file
bool Scene_Save(Scene* scene, const char* filepath)
{
    if (!scene) return false;

    // Create the root JSON Object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "scene_name", "Sample_Scene");

    // Create the "entities" array
    cJSON* entities_array = cJSON_AddArrayToObject(root, "entities");

    for (uint32_t i = 0; i < MAX_ENTITIES; i++)
    {
        if (!scene->is_active_in_hierarchy[i]) continue;

        // Create an object for this entity
        cJSON* entity_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(entity_obj, "id", i);
        cJSON_AddNumberToObject(entity_obj, "component_mask", scene->component_masks[i]);

        // --- Save Names ---
        if (scene->names && scene->names[i].name)
            cJSON_AddStringToObject(entity_obj, "name", scene->names[i].name);


        // --- Save Transforms ---
        if (scene->component_masks[i] & COMPONENT_TRANSFORM)
        {
            Transform* t = &scene->transforms[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "transform");
            cJSON_AddItemToObject(comp_obj, "position", SaveVec3(t->local_position));
            cJSON_AddItemToObject(comp_obj, "rotation", SaveQuat(t->local_rotation));
            cJSON_AddItemToObject(comp_obj, "scale",    SaveVec3(t->local_scale));
        }


        // --- Save Renderables ---
        if (scene->component_masks[i] & COMPONENT_RENDER)
        {
            RenderComponent* r = &scene->renderables[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "render");
            cJSON_AddNumberToObject(comp_obj, "mesh_id", r->mesh_id);
            cJSON_AddNumberToObject(comp_obj, "material_id", r->material_id);
        }


        // --- Save Point Lights ---
        if (scene->component_masks[i] & COMPONENT_POINT_LIGHT)
        {
            PointLightComponent* l = &scene->point_lights[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "light");
            cJSON_AddItemToObject(comp_obj, "color", SaveVec3(l->color));
            cJSON_AddNumberToObject(comp_obj, "intensity", l->intensity);
            cJSON_AddNumberToObject(comp_obj, "constant", l->constant);
            cJSON_AddNumberToObject(comp_obj, "linear", l->linear);
            cJSON_AddNumberToObject(comp_obj, "quadratic", l->quadratic);
        }


        // --- Save Colliders ---
        if (scene->component_masks[i] & COMPONENT_COLLIDER)
        {
            ColliderComponent* c = &scene->colliders[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "collider");
            cJSON_AddNumberToObject(comp_obj, "type", c->type);
            cJSON_AddBoolToObject(comp_obj, "is_trigger", c->is_trigger);
            cJSON_AddNumberToObject(comp_obj, "layer", c->collision_layer);
            cJSON_AddNumberToObject(comp_obj, "mask", c->collision_mask);
            
            // Saving the shape size so it can be rebuilt when loading
            if (c->type == COLLIDER_BOX)
                cJSON_AddItemToObject(comp_obj, "extents", SaveVec3(c->extents)); 
            else if (c->type == COLLIDER_SPHERE)
                cJSON_AddNumberToObject(comp_obj, "radius", c->radius);
            else if (c->type == COLLIDER_MESH)
            {
                cJSON_AddNumberToObject(comp_obj, "mesh_id", c->mesh_id);
                cJSON_AddItemToObject(comp_obj, "mesh_scale", SaveVec3(c->mesh_scale));
            }
        }


        // --- Save Rigidbodies ---
        if (scene->component_masks[i] & COMPONENT_RIGIDBODY)
        {
            RigidbodyComponent* rb = &scene->rigidbodies[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "rigidbody");
            cJSON_AddNumberToObject(comp_obj, "mass", rb->mass);
            cJSON_AddBoolToObject(comp_obj, "is_kinematic", rb->is_kinematic);
            cJSON_AddBoolToObject(comp_obj, "use_gravity", rb->use_gravity);
            cJSON_AddNumberToObject(comp_obj, "linear_drag", rb->linear_drag);
            cJSON_AddNumberToObject(comp_obj, "angular_drag", rb->angular_drag);
        }


        // --- Save Scripts ---
        if (scene->component_masks[i] & COMPONENT_SCRIPT)
        {
            ScriptComponent* sc = &scene->scripts[i];
            cJSON* scripts_array = cJSON_AddArrayToObject(entity_obj, "scripts");
            
            for (uint32_t s = 0; s < sc->count; s++)
            {
                cJSON* script_json = cJSON_CreateObject();
                
                // Ask bridge function to write its variables into this script_json object
                if (sc->instances[s].OnSerialize)
                    sc->instances[s].OnSerialize((Entity){i, scene}, sc->instances[s].instance_data, script_json);
                
                cJSON_AddItemToArray(scripts_array, script_json);
            }
        }


        cJSON_AddItemToArray(entities_array, entity_obj);
    }

    // Convert the JSON DOM to a formatted string
    char* json_string = cJSON_Print(root);

    // Write it to disk
    FILE* file = fopen(filepath, "w");
    if (file)
    {
        fputs(json_string, file);
        fclose(file);
    }

    // Clean up memory
    free(json_string);
    cJSON_Delete(root);

    return true;
}





// Loads a scene from a specified filepath
bool Scene_Load(Scene* scene, const char* filepath)
{
    if (!scene) return false;

    FILE* file = fopen(filepath, "r");
    if (!file) return false;

    // Find length of file and read it
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(length + 1);
    fread(buffer, 1, length, file);
    buffer[length] = '\0';
    fclose(file);

    // Parse JSON
    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) return false;

    Scene_Clear(scene);

    cJSON* entities_array = cJSON_GetObjectItemCaseSensitive(root, "entities");
    cJSON* entity_json = NULL;

    cJSON_ArrayForEach(entity_json, entities_array)
    {
        uint32_t id = cJSON_GetObjectItemCaseSensitive(entity_json, "id")->valueint;
        uint32_t mask = cJSON_GetObjectItemCaseSensitive(entity_json, "component_mask")->valueint;
        
        // Make the entity and set its component masks
        Entity e = { id, scene };
        scene->is_active_in_hierarchy[id] = true;
        scene->component_masks[id] = mask;

        // Load Name
        cJSON* name_json = cJSON_GetObjectItemCaseSensitive(entity_json, "name");
        if (name_json && scene->names)
            strncpy(scene->names[id].name, name_json->valuestring, 31);


        // --- Load Transforms ---
        if (mask & COMPONENT_TRANSFORM)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "transform");
            Transform* t = &scene->transforms[id];
            
            t->local_position = LoadVec3(cJSON_GetObjectItemCaseSensitive(comp_obj, "position"));
            t->local_rotation = LoadQuat(cJSON_GetObjectItemCaseSensitive(comp_obj, "rotation"));
            t->local_scale    = LoadVec3(cJSON_GetObjectItemCaseSensitive(comp_obj, "scale"));
            t->local_rotation_euler = QuaternionToEuler(t->local_rotation);
            t->is_dirty = true;
        }

        // --- Load Renderables ---
        if (mask & COMPONENT_RENDER)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "render");
            scene->renderables[id].mesh_id = cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_id")->valueint;
            scene->renderables[id].material_id = cJSON_GetObjectItemCaseSensitive(comp_obj, "material_id")->valueint;
        }

        // --- Load Point Lights ---
        if (mask & COMPONENT_POINT_LIGHT)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "light");
            PointLightComponent* l = &scene->point_lights[id];
            l->color = LoadVec3(cJSON_GetObjectItemCaseSensitive(comp_obj, "color"));
            l->intensity = cJSON_GetObjectItemCaseSensitive(comp_obj, "intensity")->valuedouble;
            l->constant = cJSON_GetObjectItemCaseSensitive(comp_obj, "constant")->valuedouble;
            l->linear = cJSON_GetObjectItemCaseSensitive(comp_obj, "linear")->valuedouble;;
            l->quadratic = cJSON_GetObjectItemCaseSensitive(comp_obj, "quadratic")->valuedouble;
        }

        // --- Load Colliders ---
        // Call the physics creation functions to rebuild pointers
        if (mask & COMPONENT_COLLIDER)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "collider");
            
            int type = cJSON_GetObjectItemCaseSensitive(comp_obj, "type")->valueint;
            bool is_trigger = cJSON_GetObjectItemCaseSensitive(comp_obj, "is_trigger")->valueint;
            int layer = cJSON_GetObjectItemCaseSensitive(comp_obj, "layer")->valueint;
            int col_mask = cJSON_GetObjectItemCaseSensitive(comp_obj, "mask")->valueint;

            if (type == COLLIDER_BOX)
            {
                Vector3 extents = LoadVec3(cJSON_GetObjectItemCaseSensitive(comp_obj, "extents"));
                Entity_AddColliderBox(e, extents, is_trigger);
            } 
            else if (type == COLLIDER_SPHERE)
            {
                float radius = cJSON_GetObjectItemCaseSensitive(comp_obj, "radius")->valuedouble;
                Entity_AddColliderSphere(e, radius, is_trigger);
            }
            else if (type == COLLIDER_MESH)
            {
                MeshHandle mesh = { (uint32_t)cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_id")->valueint };
                Vector3 mesh_scale = LoadVec3(cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_scale"));
                Entity_AddColliderMesh(e, mesh, is_trigger);
            }
            
            // Reapply layers
            Collider_SetLayerAndMask(e, layer, col_mask);
        }

        // --- Load Rigidboides ---
        // Must be done after loading colliders so it has a shape to attach to
        if (mask & COMPONENT_RIGIDBODY)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "rigidbody");
            
            float mass = cJSON_GetObjectItemCaseSensitive(comp_obj, "mass")->valuedouble;
            Entity_AddRigidbody(e, mass);
            
            // Re-apply specific settings
            RigidbodyComponent* rb = Entity_GetRigidbody(e);
            rb->is_kinematic = cJSON_GetObjectItemCaseSensitive(comp_obj, "is_kinematic")->valueint;
            Rigidbody_SetKinematic(e, rb->is_kinematic);
            
            rb->use_gravity = cJSON_GetObjectItemCaseSensitive(comp_obj, "use_gravity")->valueint;
            Rigidbody_SetGravity(e, rb->use_gravity);
        }


        // --- Load Scripts ---
        if (mask & COMPONENT_SCRIPT)
        {
            cJSON* scripts_array = cJSON_GetObjectItemCaseSensitive(entity_json, "scripts");
            cJSON* script_json = NULL;
            
            cJSON_ArrayForEach(script_json, scripts_array)
            {
                cJSON* class_node = cJSON_GetObjectItemCaseSensitive(script_json, "class_name");
                
                if (class_node && cJSON_IsString(class_node))
                {
                    // Tell Bridge function to build this class and load the variables!
                    Bridge_SpawnScript(e, class_node->valuestring, script_json);
                }
            }
        }
    }

    cJSON_Delete(root);
    Scene_UpdateTransforms(scene); 
    
    return true;
}