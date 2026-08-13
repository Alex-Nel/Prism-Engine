#include "scene.h"
#include <cJSON.h>
#include <stdlib.h>
#include <string.h>



static cJSON* SaveVec2(Vector2 v)
{
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.x));
    cJSON_AddItemToArray(arr, cJSON_CreateNumber(v.y));
    return arr;
}

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

static Vector2 LoadVec2(cJSON* arr)
{
    if (!arr || cJSON_GetArraySize(arr) < 2)
        return (Vector2){0,0};

    return (Vector2){
        (float)cJSON_GetArrayItem(arr, 0)->valuedouble,
        (float)cJSON_GetArrayItem(arr, 1)->valuedouble
    };
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

    cJSON_AddNumberToObject(root, "ambient_r", scene->ambient_color.r);
    cJSON_AddNumberToObject(root, "ambient_g", scene->ambient_color.g);
    cJSON_AddNumberToObject(root, "ambient_b", scene->ambient_color.b);
    cJSON_AddNumberToObject(root, "ambient_a", scene->ambient_color.a);
    cJSON_AddNumberToObject(root, "ambient_illumination", scene->ambient_illumination);

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


        // --- Save Mesh Renderers ---
        if (scene->component_masks[i] & COMPONENT_MESH_RENDERER)
        {
            MeshRendererComponent* r = &scene->mesh_renderers[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "mesh_renderer");

            cJSON_AddStringToObject(comp_obj, "mesh_name", r->mesh->name);

            if (r->material && r->material->albedo_texture)
            {
                cJSON_AddStringToObject(comp_obj, "texture_name", r->material->albedo_texture->name);
                cJSON_AddItemToObject(comp_obj, "tint", SaveColor(r->material->properties.albedo_tint));
                cJSON_AddNumberToObject(comp_obj, "shininess", r->material->properties.metallic_factor);
                cJSON_AddNumberToObject(comp_obj, "specular_strength", r->material->properties.roughness_factor);
            }
        }


        // --- Save Skinned Mesh Renderers ---
        if (scene->component_masks[i] & COMPONENT_SKINNED_MESH_RENDERER)
        {
            SkinnedMeshRendererComponent* r = &scene->skinned_mesh_renderers[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "skinned_mesh_renderer");

            cJSON_AddStringToObject(comp_obj, "mesh_name", r->mesh->name);

            if (r->material && r->material->albedo_texture)
            {
                cJSON_AddStringToObject(comp_obj, "texture_name", r->material->albedo_texture->name);
                cJSON_AddItemToObject(comp_obj, "tint", SaveColor(r->material->properties.albedo_tint));
                cJSON_AddNumberToObject(comp_obj, "shininess", r->material->properties.metallic_factor);
                cJSON_AddNumberToObject(comp_obj, "specular_strength", r->material->properties.roughness_factor);
            }
        }


        // --- Save Point Lights ---
        if (scene->component_masks[i] & COMPONENT_LIGHT)
        {
            LightComponent* l = &scene->lights[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "light");
            cJSON_AddNumberToObject(comp_obj, "type", l->type);
            cJSON_AddItemToObject(comp_obj, "color", SaveColor(l->color));
            cJSON_AddNumberToObject(comp_obj, "intensity", l->intensity);
            cJSON_AddNumberToObject(comp_obj, "ambient_strength", l->ambient_strength);
            cJSON_AddNumberToObject(comp_obj, "constant", l->constant);
            cJSON_AddNumberToObject(comp_obj, "linear", l->linear);
            cJSON_AddNumberToObject(comp_obj, "quadratic", l->quadratic);
            cJSON_AddNumberToObject(comp_obj, "inner_cut_off", l->inner_cut_off);
            cJSON_AddNumberToObject(comp_obj, "outer_cut_off", l->outer_cut_off);
            cJSON_AddNumberToObject(comp_obj, "shadow_box_size", l->shadow_box_size);
            cJSON_AddNumberToObject(comp_obj, "shadow_cascade_count", l->shadow_cascade_count);
            cJSON_AddNumberToObject(comp_obj, "shadow_max_distance", l->shadow_max_distance);
            cJSON_AddNumberToObject(comp_obj, "cascade_split_lambda", l->cascade_split_lambda);
            cJSON_AddNumberToObject(comp_obj, "cascade_blend_fraction", l->cascade_blend_fraction);
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
            cJSON_AddBoolToObject(comp_obj, "active", al->is_active);
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

        // TODO: add line and sprite renderers

        // --- Save Local IBL Probes ---
        if (scene->component_masks[i] & COMPONENT_REFLECTION_PROBE)
        {
            ReflectionProbeComponent* probe = &scene->reflection_probes[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "reflection_probe");
            cJSON_AddBoolToObject(comp_obj, "active", probe->is_active);
            cJSON_AddItemToObject(comp_obj, "box_extents", SaveVec3(probe->box_extents));
            cJSON_AddNumberToObject(comp_obj, "blend_distance", probe->blend_distance);
            cJSON_AddNumberToObject(comp_obj, "priority", probe->priority);
            cJSON_AddNumberToObject(comp_obj, "capture_resolution", probe->capture_resolution);
        }


        // --- Save UI Canvas ---
        if (scene->component_masks[i] & COMPONENT_UI_CANVAS)
        {
            UICanvasComponent* canvas = &scene->ui_canvases[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "ui_canvas");
            cJSON_AddBoolToObject(comp_obj, "active", canvas->is_active);
            cJSON_AddNumberToObject(comp_obj, "render_mode", canvas->render_mode);
            cJSON_AddNumberToObject(comp_obj, "sort_order", canvas->sort_order);
            cJSON_AddNumberToObject(comp_obj, "scale_mode", canvas->scale_mode);
            cJSON_AddItemToObject(comp_obj, "reference_resolution", SaveVec2(canvas->reference_resolution));
            cJSON_AddNumberToObject(comp_obj, "match_width_or_height", canvas->match_width_or_height);
            cJSON_AddBoolToObject(comp_obj, "blocks_raycasts", canvas->blocks_raycasts);
        }


        // --- Save UI Rect Transform ---
        if (scene->component_masks[i] & COMPONENT_UI_RECT_TRANSFORM)
        {
            RectTransformComponent* rect = &scene->rect_transforms[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "rect_transform");
            cJSON_AddItemToObject(comp_obj, "anchor_min", SaveVec2(rect->anchor_min));
            cJSON_AddItemToObject(comp_obj, "anchor_max", SaveVec2(rect->anchor_max));
            cJSON_AddItemToObject(comp_obj, "pivot", SaveVec2(rect->pivot));
            cJSON_AddItemToObject(comp_obj, "size_delta", SaveVec2(rect->size_delta));
            cJSON_AddItemToObject(comp_obj, "anchored_position", SaveVec2(rect->anchored_position));
            cJSON_AddItemToObject(comp_obj, "local_scale", SaveVec2(rect->local_scale));
            cJSON_AddNumberToObject(comp_obj, "local_rotation_z", rect->local_rotation_z);
        }


        // --- Save UI image ---
        if (scene->component_masks[i] & COMPONENT_UI_IMAGE)
        {
            UIImageComponent* image = &scene->ui_images[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "ui_image");
            cJSON_AddBoolToObject(comp_obj, "active", image->is_active);
            if (image->texture)
                cJSON_AddStringToObject(comp_obj, "texture_name", image->texture->name);
            cJSON_AddItemToObject(comp_obj, "color", SaveColor(image->color));
            cJSON_AddBoolToObject(comp_obj, "raycast_target", image->raycast_target);
        }


        // --- Save UI Text ---
        if (scene->component_masks[i] & COMPONENT_UI_TEXT)
        {
            UITextComponent* text = &scene->ui_texts[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "ui_text");
            cJSON_AddBoolToObject(comp_obj, "active", text->is_active);
            cJSON_AddStringToObject(comp_obj, "text", text->text);
            if (text->font)
                cJSON_AddStringToObject(comp_obj, "font_name", text->font->name);
            cJSON_AddItemToObject(comp_obj, "color", SaveColor(text->color));
            cJSON_AddNumberToObject(comp_obj, "alignment", text->alignment);
            cJSON_AddNumberToObject(comp_obj, "font_size", text->font_size);
            cJSON_AddBoolToObject(comp_obj, "wrap", text->wrap);
            cJSON_AddBoolToObject(comp_obj, "raycast_target", text->raycast_target);
        }


        // --- Save UI Button ---
        if (scene->component_masks[i] & COMPONENT_UI_BUTTON)
        {
            UIButtonComponent* button = &scene->ui_buttons[i];
            cJSON* comp_obj = cJSON_AddObjectToObject(entity_obj, "ui_button");
            cJSON_AddBoolToObject(comp_obj, "active", button->is_active);
            cJSON_AddBoolToObject(comp_obj, "interactable", button->interactable);
            cJSON_AddItemToObject(comp_obj, "color_normal", SaveColor(button->color_normal));
            cJSON_AddItemToObject(comp_obj, "color_hovered", SaveColor(button->color_hovered));
            cJSON_AddItemToObject(comp_obj, "color_pressed", SaveColor(button->color_pressed));
            cJSON_AddItemToObject(comp_obj, "color_disabled", SaveColor(button->color_disabled));
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

    cJSON* ambient_r = cJSON_GetObjectItemCaseSensitive(root, "ambient_r");
    cJSON* ambient_g = cJSON_GetObjectItemCaseSensitive(root, "ambient_g");
    cJSON* ambient_b = cJSON_GetObjectItemCaseSensitive(root, "ambient_b");
    cJSON* ambient_a = cJSON_GetObjectItemCaseSensitive(root, "ambient_a");
    cJSON* ambient_int = cJSON_GetObjectItemCaseSensitive(root, "ambient_illumination");

    if (ambient_r && ambient_g && ambient_b)
    {
        scene->ambient_color.r = (float)ambient_r->valuedouble;
        scene->ambient_color.g = (float)ambient_g->valuedouble;
        scene->ambient_color.b = (float)ambient_b->valuedouble;
        scene->ambient_color.a = ambient_a ? (float)ambient_a->valuedouble : 1.0f;
    }
    if (ambient_int)
        scene->ambient_illumination = (float)ambient_int->valuedouble;

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
        if (mask & COMPONENT_MESH_RENDERER)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "mesh_renderer");
            MeshRendererComponent* r = &scene->mesh_renderers[id];
            
            cJSON* mesh_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_name");

            r->mesh = Asset_GetMeshByName(mesh_name->valuestring);
            
            cJSON* tex_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "texture_name");
            Texture* tex = tex_name ? Asset_GetTextureByName(tex_name->valuestring) : NULL;
            
            r->material = Asset_CreateMaterial(NULL, tex);
            
            cJSON* tint = cJSON_GetObjectItemCaseSensitive(comp_obj, "tint");
            if (tint) r->material->properties.albedo_tint = LoadColor(tint);
        }


        // --- Load Renderables ---
        if (mask & COMPONENT_SKINNED_MESH_RENDERER)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "skinned_mesh_renderer");
            SkinnedMeshRendererComponent* r = &scene->skinned_mesh_renderers[id];
            
            cJSON* mesh_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "mesh_name");

            r->mesh = Asset_GetSkinnedMeshByName(mesh_name->valuestring);
            
            cJSON* tex_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "texture_name");
            Texture* tex = tex_name ? Asset_GetTextureByName(tex_name->valuestring) : NULL;
            
            r->material = Asset_CreateMaterial(NULL, tex);
            
            cJSON* tint = cJSON_GetObjectItemCaseSensitive(comp_obj, "tint");
            if (tint) r->material->properties.albedo_tint = LoadColor(tint);
        }


        // --- Load Point Lights ---
        if (mask & COMPONENT_LIGHT)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "light");
            LightComponent* l = &scene->lights[id];
            l->type = cJSON_GetObjectItemCaseSensitive(comp_obj, "type")->valuedouble;
            l->color = LoadColor(cJSON_GetObjectItemCaseSensitive(comp_obj, "color"));
            l->intensity = cJSON_GetObjectItemCaseSensitive(comp_obj, "intensity")->valuedouble;
            l->ambient_strength = cJSON_GetObjectItemCaseSensitive(comp_obj, "ambient_strength")->valuedouble;
            l->constant = cJSON_GetObjectItemCaseSensitive(comp_obj, "constant")->valuedouble;
            l->linear = cJSON_GetObjectItemCaseSensitive(comp_obj, "linear")->valuedouble;
            l->quadratic = cJSON_GetObjectItemCaseSensitive(comp_obj, "quadratic")->valuedouble;
            l->inner_cut_off = cJSON_GetObjectItemCaseSensitive(comp_obj, "inner_cut_off")->valuedouble;
            l->outer_cut_off = cJSON_GetObjectItemCaseSensitive(comp_obj, "outer_cut_off")->valuedouble;
            cJSON* shadow_box = cJSON_GetObjectItemCaseSensitive(comp_obj, "shadow_box_size");
            l->shadow_box_size = shadow_box ? (float)shadow_box->valuedouble : ((l->type == LIGHT_DIRECTIONAL) ? 20.0f : 0.0f);
            cJSON* cascade_count = cJSON_GetObjectItemCaseSensitive(comp_obj, "shadow_cascade_count");
            l->shadow_cascade_count = cascade_count ? (uint8_t)cascade_count->valuedouble : SHADOW_CASCADE_COUNT_DEFAULT;
            cJSON* max_dist = cJSON_GetObjectItemCaseSensitive(comp_obj, "shadow_max_distance");
            l->shadow_max_distance = max_dist ? (float)max_dist->valuedouble : 100.0f;
            cJSON* split_lambda = cJSON_GetObjectItemCaseSensitive(comp_obj, "cascade_split_lambda");
            l->cascade_split_lambda = split_lambda ? (float)split_lambda->valuedouble : 0.5f;
            cJSON* blend_fraction = cJSON_GetObjectItemCaseSensitive(comp_obj, "cascade_blend_fraction");
            l->cascade_blend_fraction = blend_fraction ? (float)blend_fraction->valuedouble : 0.12f;
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
            al->is_active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active")->valueint;
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


        // TODO: Add sprite and line renderers


        // --- Load Local IBL Probes ---
        if (mask & COMPONENT_REFLECTION_PROBE)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "reflection_probe");
            if (comp_obj)
            {
                Vector3 extents = LoadVec3(cJSON_GetObjectItemCaseSensitive(comp_obj, "box_extents"));
                cJSON* blend = cJSON_GetObjectItemCaseSensitive(comp_obj, "blend_distance");
                cJSON* resolution = cJSON_GetObjectItemCaseSensitive(comp_obj, "capture_resolution");
                Entity_AddReflectionProbe(e, extents, blend ? (float)blend->valuedouble : 1.0f, resolution ? (uint32_t)resolution->valueint : 128 );

                ReflectionProbeComponent* probe = Entity_GetReflectionProbe(e);
                cJSON* active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active");
                cJSON* priority = cJSON_GetObjectItemCaseSensitive(comp_obj, "priority");
                if (active) probe->is_active = active->valueint != 0;
                if (priority) probe->priority = priority->valueint;
                probe->dirty = true;
                probe->captured = false;
            }
        }


        // --- Loads a rect transform ---
        if (mask & COMPONENT_UI_RECT_TRANSFORM)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "rect_transform");
            Entity_AddRectTransform(e);
            RectTransformComponent* rect = Entity_GetRectTransform(e);
            if (comp_obj && rect)
            {
                rect->anchor_min = LoadVec2(cJSON_GetObjectItemCaseSensitive(comp_obj, "anchor_min"));
                rect->anchor_max = LoadVec2(cJSON_GetObjectItemCaseSensitive(comp_obj, "anchor_max"));
                rect->pivot = LoadVec2(cJSON_GetObjectItemCaseSensitive(comp_obj, "pivot"));
                rect->size_delta = LoadVec2(cJSON_GetObjectItemCaseSensitive(comp_obj, "size_delta"));
                rect->anchored_position = LoadVec2(cJSON_GetObjectItemCaseSensitive(comp_obj, "anchored_position"));
                cJSON* local_scale = cJSON_GetObjectItemCaseSensitive(comp_obj, "local_scale");
                if (local_scale)
                    rect->local_scale = LoadVec2(local_scale);
                cJSON* rot = cJSON_GetObjectItemCaseSensitive(comp_obj, "local_rotation_z");
                if (rot)
                    rect->local_rotation_z = (float)rot->valuedouble;
                rect->is_dirty = true;
            }
        }


        // --- Loads a UI Canvas ---
        if (mask & COMPONENT_UI_CANVAS)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "ui_canvas");
            Entity_AddUICanvas(e);
            UICanvasComponent* canvas = Entity_GetUICanvas(e);
            if (comp_obj && canvas)
            {
                cJSON* active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active");
                if (active) canvas->is_active = active->valueint != 0;
                cJSON* render_mode = cJSON_GetObjectItemCaseSensitive(comp_obj, "render_mode");
                if (render_mode) canvas->render_mode = (UICanvasRenderMode)render_mode->valueint;
                cJSON* sort_order = cJSON_GetObjectItemCaseSensitive(comp_obj, "sort_order");
                if (sort_order) canvas->sort_order = sort_order->valueint;
                cJSON* scale_mode = cJSON_GetObjectItemCaseSensitive(comp_obj, "scale_mode");
                if (scale_mode) canvas->scale_mode = (UICanvasScaleMode)scale_mode->valueint;
                cJSON* reference = cJSON_GetObjectItemCaseSensitive(comp_obj, "reference_resolution");
                if (reference) canvas->reference_resolution = LoadVec2(reference);
                cJSON* match = cJSON_GetObjectItemCaseSensitive(comp_obj, "match_width_or_height");
                if (match) canvas->match_width_or_height = (float)match->valuedouble;
                cJSON* blocks = cJSON_GetObjectItemCaseSensitive(comp_obj, "blocks_raycasts");
                if (blocks) canvas->blocks_raycasts = blocks->valueint != 0;
            }
        }


        // --- Loads a UI Image ---
        if (mask & COMPONENT_UI_IMAGE)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "ui_image");
            Texture* tex = NULL;
            if (comp_obj)
            {
                cJSON* tex_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "texture_name");
                if (tex_name && cJSON_IsString(tex_name))
                    tex = Asset_GetTextureByName(tex_name->valuestring);
            }
            Entity_AddUIImage(e, tex);
            UIImageComponent* image = Entity_GetUIImage(e);
            if (comp_obj && image)
            {
                cJSON* active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active");
                if (active) image->is_active = active->valueint != 0;
                cJSON* color = cJSON_GetObjectItemCaseSensitive(comp_obj, "color");
                if (color) image->color = LoadColor(color);
                cJSON* raycast = cJSON_GetObjectItemCaseSensitive(comp_obj, "raycast_target");
                if (raycast) image->raycast_target = raycast->valueint != 0;
            }
        }


        // --- Loads a UI Text ---
        if (mask & COMPONENT_UI_TEXT)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "ui_text");
            const char* text_value = "";
            Font* font = NULL;
            if (comp_obj)
            {
                cJSON* text_json = cJSON_GetObjectItemCaseSensitive(comp_obj, "text");
                if (text_json && cJSON_IsString(text_json))
                    text_value = text_json->valuestring;
                cJSON* font_name = cJSON_GetObjectItemCaseSensitive(comp_obj, "font_name");
                if (font_name && cJSON_IsString(font_name))
                    font = Asset_GetFontByName(font_name->valuestring);
            }
            Entity_AddUIText(e, text_value, font);
            UITextComponent* text = Entity_GetUIText(e);
            if (comp_obj && text)
            {
                cJSON* active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active");
                if (active) text->is_active = active->valueint != 0;
                cJSON* color = cJSON_GetObjectItemCaseSensitive(comp_obj, "color");
                if (color) text->color = LoadColor(color);
                cJSON* alignment = cJSON_GetObjectItemCaseSensitive(comp_obj, "alignment");
                if (alignment) text->alignment = (UITextAlignment)alignment->valueint;
                cJSON* font_size = cJSON_GetObjectItemCaseSensitive(comp_obj, "font_size");
                if (font_size) text->font_size = (float)font_size->valuedouble;
                cJSON* wrap = cJSON_GetObjectItemCaseSensitive(comp_obj, "wrap");
                if (wrap) text->wrap = wrap->valueint != 0;
                cJSON* raycast = cJSON_GetObjectItemCaseSensitive(comp_obj, "raycast_target");
                if (raycast) text->raycast_target = raycast->valueint != 0;
            }
        }


        // --- Loads a UI Button ---
        if (mask & COMPONENT_UI_BUTTON)
        {
            cJSON* comp_obj = cJSON_GetObjectItemCaseSensitive(entity_json, "ui_button");
            Entity_AddUIButton(e);
            UIButtonComponent* button = Entity_GetUIButton(e);
            if (comp_obj && button)
            {
                cJSON* active = cJSON_GetObjectItemCaseSensitive(comp_obj, "active");
                if (active) button->is_active = active->valueint != 0;
                cJSON* interactable = cJSON_GetObjectItemCaseSensitive(comp_obj, "interactable");
                if (interactable) button->interactable = interactable->valueint != 0;
                cJSON* c_normal = cJSON_GetObjectItemCaseSensitive(comp_obj, "color_normal");
                if (c_normal) button->color_normal = LoadColor(c_normal);
                cJSON* c_hovered = cJSON_GetObjectItemCaseSensitive(comp_obj, "color_hovered");
                if (c_hovered) button->color_hovered = LoadColor(c_hovered);
                cJSON* c_pressed = cJSON_GetObjectItemCaseSensitive(comp_obj, "color_pressed");
                if (c_pressed) button->color_pressed = LoadColor(c_pressed);
                cJSON* c_disabled = cJSON_GetObjectItemCaseSensitive(comp_obj, "color_disabled");
                if (c_disabled) button->color_disabled = LoadColor(c_disabled);
            }
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