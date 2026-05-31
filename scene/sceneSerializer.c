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

static cJSON* SaveColor(Color c)
{
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(c.r));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(c.g));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(c.b));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(c.a));
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

static Color LoadColor(cJSON* arr)
{
    Color c = {1.0f, 1.0f, 1.0f, 1.0f}; // Default to solid white
    
    if (arr && cJSON_IsArray(arr) && cJSON_GetArraySize(arr) >= 3)
    {
        c.r = (float)cJSON_GetArrayItem(arr, 0)->valuedouble;
        c.g = (float)cJSON_GetArrayItem(arr, 1)->valuedouble;
        c.b = (float)cJSON_GetArrayItem(arr, 2)->valuedouble;
        
        // If it's a new save file, it has an alpha channel
        if (cJSON_GetArraySize(arr) >= 4)
            c.a = (float)cJSON_GetArrayItem(arr, 3)->valuedouble;
        // If it's an old save file that uses Vector3, default alpha to 1.0
        else
            c.a = 1.0f;
    }

    return c;
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


        // --- Save Cameras ---
        if (scene->component_masks[i] & COMPONENT_CAMERA)
        {
            CameraComponent* cam = &scene->cameras[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "camera");
            cJSON_AddNumberToObject(comp_obj, "fov", cam->fov);
            cJSON_AddNumberToObject(comp_obj, "nearZ", cam->nearZ);
            cJSON_AddNumberToObject(comp_obj, "farZ", cam->farZ);
        }


        // --- Save Renderables ---
        if (scene->component_masks[i] & COMPONENT_RENDER)
        {
            RenderComponent* r = &scene->renderables[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "render");

            if (r->model)
            {
                cJSON_AddStringToObject(comp_obj, "model_name", r->model->name);
                // TODO: Update material naming system to serialize material overrides
            }
            else if (r->single_mesh)
            {
                cJSON_AddStringToObject(comp_obj, "mesh_name", r->single_mesh->name);

                if (r->single_material && r->single_material->diffuse_texture)
                {
                    cJSON_AddStringToObject(comp_obj, "texture_name", r->single_material->diffuse_texture->name);
                    cJSON_AddItemToObject(comp_obj, "tint", SaveColor(r->single_material->properties.tint_color));
                    cJSON_AddNumberToObject(comp_obj, "shininess", r->single_material->properties.shininess);
                    cJSON_AddNumberToObject(comp_obj, "specular_strength", r->single_material->properties.specular_strength);
                }
            }
        }


        // --- Save Point Lights ---
        if (scene->component_masks[i] & COMPONENT_POINT_LIGHT)
        {
            PointLightComponent* l = &scene->point_lights[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "light");
            cJSON_AddItemToObject(comp_obj, "color", SaveColor(l->color));
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
            cJSON_AddBoolToObject(comp_obj, "is_convex", c->is_convex);
            cJSON_AddNumberToObject(comp_obj, "layer", c->collision_layer);
            cJSON_AddNumberToObject(comp_obj, "mask", c->collision_mask);
            
            // Saving the shape size so it can be rebuilt when loading
            if (c->type == COLLIDER_BOX)
                cJSON_AddItemToObject(comp_obj, "extents", SaveVec3(c->extents)); 
            else if (c->type == COLLIDER_SPHERE)
                cJSON_AddNumberToObject(comp_obj, "radius", c->radius);
            else if (c->type == COLLIDER_MESH && c->mesh_ptr)
            {
                cJSON_AddStringToObject(comp_obj, "mesh_name", c->mesh_ptr->name);
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

            cJSON_AddBoolToObject(comp_obj, "freeze_rot_x", rb->freeze_rot_x);
            cJSON_AddBoolToObject(comp_obj, "freeze_rot_y", rb->freeze_rot_y);
            cJSON_AddBoolToObject(comp_obj, "freeze_rot_z", rb->freeze_rot_z);
        }


        // --- Save Audio Listener ---
        if (scene->component_masks[i] & COMPONENT_AUDIO_LISTENER)
        {
            AudioListenerComponent* al = &scene->audio_listeners[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "audio_listener");
            cJSON_AddBoolToObject(comp_obj, "active", al->active);
        }


        // --- Save Audio Source ---
        if (scene->component_masks[i] & COMPONENT_AUDIO_SOURCE)
        {
            AudioSourceComponent* as = &scene->audio_sources[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "audio_source");
            cJSON_AddNumberToObject(comp_obj, "volume", as->volume);
            cJSON_AddNumberToObject(comp_obj, "pitch", as->pitch);
            cJSON_AddBoolToObject(comp_obj, "loop", as->loop);
            cJSON_AddBoolToObject(comp_obj, "play_on_awake", as->play_on_awake);
            cJSON_AddBoolToObject(comp_obj, "is_spatial", as->is_spatial);
            cJSON_AddNumberToObject(comp_obj, "min_distance", as->min_distance);
            cJSON_AddNumberToObject(comp_obj, "max_distance", as->max_distance);
            // TODO: Save audio clip name instead of handle
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


        // --- Load Cameras ---
        if (mask & COMPONENT_CAMERA)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "camera");
            CameraComponent* cam = &scene->cameras[id];
            cam->fov = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "fov")->valuedouble;
            cam->nearZ = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "nearZ")->valuedouble;
            cam->farZ = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "farZ")->valuedouble;
            cam->is_dirty = true;
        }


        // --- Load Renderables ---
        if (mask & COMPONENT_RENDER)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "render");
            RenderComponent* r = &scene->renderables[id];
            
            cJSON* model_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "model_name");
            cJSON* mesh_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_name");

            if (model_name) 
            {
                r->model = Asset_GetModelByName(model_name->valuestring);
            } 
            else if (mesh_name) 
            {
                r->single_mesh = Asset_GetMeshByName(mesh_name->valuestring);
                
                cJSON* tex_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "texture_name");
                Texture* tex = tex_name ? Asset_GetTextureByName(tex_name->valuestring) : NULL;
                
                r->single_material = Asset_CreateMaterial(NULL, tex);
                
                cJSON* tint = cJSON_GetObjectItemCaseSensitive(comp_obj, "tint");
                if (tint) r->single_material->properties.tint_color = LoadColor(tint);
            }
        }


        // --- Load Point Lights ---
        if (mask & COMPONENT_POINT_LIGHT)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "light");
            PointLightComponent* l = &scene->point_lights[id];
            l->color = LoadColor(cJSON_GetObjectItemCaseSensitive(comp_obj, "color"));
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
            bool is_convex = cJSON_GetObjectItemCaseSensitive(comp_obj, "is_convex")->valueint;
            
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
                cJSON* mesh_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_name");
                if (mesh_name)
                {
                    Mesh* mesh_ptr = Asset_GetMeshByName(mesh_name->valuestring);
                    Entity_AddColliderMesh(e, mesh_ptr, is_trigger, is_convex);
                }
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

            rb->linear_drag = cJSON_GetObjectItemCaseSensitive(comp_obj, "linear_drag")->valuedouble;
            rb->angular_drag = cJSON_GetObjectItemCaseSensitive(comp_obj, "angular_drag")->valuedouble;

            // Load Freeze Variables
            cJSON* f_x = cJSON_GetObjectItemCaseSensitive(comp_obj, "freeze_rot_x");
            cJSON* f_y = cJSON_GetObjectItemCaseSensitive(comp_obj, "freeze_rot_y");
            cJSON* f_z = cJSON_GetObjectItemCaseSensitive(comp_obj, "freeze_rot_z");
            
            if (f_x) rb->freeze_rot_x = f_x->valueint;
            if (f_y) rb->freeze_rot_y = f_y->valueint;
            if (f_z) rb->freeze_rot_z = f_z->valueint;

            if (mask & COMPONENT_COLLIDER)
            {
                ColliderComponent* c = &scene->colliders[id];
                if (c->physics_handle)
                    Physics_SetRotationConstraints(c->physics_handle, rb->freeze_rot_x, rb->freeze_rot_y, rb->freeze_rot_z);
            }
        }


        // --- Load Audio Listener ---
        if (mask & COMPONENT_AUDIO_LISTENER)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "audio_listener");
            
            Entity_AddAudioListener(e); // Initialize standard defaults
            
            AudioListenerComponent* al = &scene->audio_listeners[id];
            al->active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active")->valueint;
        }


        // --- Load Audio Source ---
        if (mask & COMPONENT_AUDIO_SOURCE)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "audio_source");
            
            Entity_AddAudioSource(e); // Initialize standard defaults
            
            AudioSourceComponent* as = &scene->audio_sources[id];
            
            as->volume = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "volume")->valuedouble;
            as->pitch = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "pitch")->valuedouble;
            as->loop = cJSON_GetObjectItemCaseSensitive(comp_obj, "loop")->valueint;
            as->play_on_awake = cJSON_GetObjectItemCaseSensitive(comp_obj, "play_on_awake")->valueint;
            as->is_spatial = cJSON_GetObjectItemCaseSensitive(comp_obj, "is_spatial")->valueint;
            as->min_distance = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "min_distance")->valuedouble;
            as->max_distance = (float)cJSON_GetObjectItemCaseSensitive(comp_obj, "max_distance")->valuedouble;
            
            // TODO:
            // Re-resolve the Audio Clip via String (assuming it's saved as a string)
            // cJSON* clip_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "clip_name");
            // if (clip_name) {
            //     as->clip = Asset_GetAudioClipByName(clip_name->valuestring);
            // }
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