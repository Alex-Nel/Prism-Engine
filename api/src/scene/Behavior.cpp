#include "../../include/scene/Behavior.hpp"


extern "C"
{
    #include "../../../scene/scene.h"
}


#include <cJSON.h>


namespace Prism
{
    // ==========================================
    // Serialization Implementation
    // ==========================================

    void Behavior::Engine_Serialize(cJSON* json)
    {
        cJSON_AddStringToObject(json, "class_name", script_class_name.c_str());

        for (const auto& pair : m_Properties)
        {
            const std::string& name = pair.first;
            if (std::holds_alternative<float*>(pair.second))
                cJSON_AddNumberToObject(json, name.c_str(), *std::get<float*>(pair.second));
            else if (std::holds_alternative<int*>(pair.second))
                cJSON_AddNumberToObject(json, name.c_str(), *std::get<int*>(pair.second));
            else if (std::holds_alternative<bool*>(pair.second))
                cJSON_AddBoolToObject(json, name.c_str(), *std::get<bool*>(pair.second));
        }
    }

    
    void Behavior::Engine_Deserialize(cJSON* json)
    {
        for (const auto& pair : m_Properties)
        {
            const std::string& name = pair.first;
            cJSON* item = cJSON_GetObjectItem(json, name.c_str());
            
            if (!item) continue;

            if (std::holds_alternative<float*>(pair.second) && cJSON_IsNumber(item))
                *std::get<float*>(pair.second) = item->valuedouble;
            else if (std::holds_alternative<int*>(pair.second) && cJSON_IsNumber(item))
                *std::get<int*>(pair.second) = item->valueint;
            else if (std::holds_alternative<bool*>(pair.second) && cJSON_IsBool(item))
                *std::get<bool*>(pair.second) = cJSON_IsTrue(item);
        }
    }



    // ==========================================
    // Bridge functions for scripts
    // ==========================================
    
    inline void Bridge_OnStart(::Entity e, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnStart();
    }
    inline void Bridge_OnUpdate(::Entity e, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnUpdate();
    }
    inline void Bridge_OnDestroy(::Entity e, void* data) { 
        if (data) { 
            Behavior* b = static_cast<Behavior*>(data);
            b->OnDestroy(); 
            delete b; 
        } 
    }
    inline void Bridge_OnEnable(::Entity e, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnEnable();
    }
    inline void Bridge_OnDisable(::Entity e, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnDisable();
    }

    inline void Bridge_OnTriggerEnter(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnTriggerEnter(Entity(other.id, other.scene));
    }
    inline void Bridge_OnTriggerStay(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnTriggerStay(Entity(other.id, other.scene));
    }
    inline void Bridge_OnTriggerExit(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnTriggerExit(Entity(other.id, other.scene));
    }
    
    inline void Bridge_OnCollisionEnter(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnCollisionEnter(Entity(other.id, other.scene));
    }
    inline void Bridge_OnCollisionStay(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnCollisionStay(Entity(other.id, other.scene));
    }
    inline void Bridge_OnCollisionExit(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnCollisionExit(Entity(other.id, other.scene));
    }

    inline void Bridge_OnSerialize(::Entity e, void* data, cJSON* json) {
        if (data)
            static_cast<Behavior*>(data)->Engine_Serialize(json);
    }
    inline void Bridge_OnDeserialize(::Entity e, void* data, cJSON* json) {
        if (data)
            static_cast<Behavior*>(data)->Engine_Deserialize(json);
    }



    // ==========================================
    // Internal Helpers
    // ==========================================
    
    void BindBehavior_Internal(uint32_t entity_id, void* scene_ptr, Behavior* instance)
    {
        ::ScriptInstance script = {0};
        script.instance_data = instance;
        
        script.OnStart = Bridge_OnStart;
        script.OnUpdate = reinterpret_cast<::ScriptUpdateFunc>(Bridge_OnUpdate);
        script.OnDestroy = Bridge_OnDestroy;
        script.OnEnable = Bridge_OnEnable;
        script.OnDisable = Bridge_OnDisable;
        
        script.OnTriggerEnter = Bridge_OnTriggerEnter;
        script.OnTriggerStay = Bridge_OnTriggerStay;
        script.OnTriggerExit = Bridge_OnTriggerExit;
        script.OnCollisionEnter = Bridge_OnCollisionEnter;
        script.OnCollisionStay = Bridge_OnCollisionStay;
        script.OnCollisionExit = Bridge_OnCollisionExit;

        script.OnSerialize = Bridge_OnSerialize;
        script.OnDeserialize = Bridge_OnDeserialize;

        // Reconstruct the C entity
        ::Entity raw_e = { entity_id, static_cast<::Scene*>(scene_ptr) };
        ::Entity_BindScript(raw_e, script);
    }


    std::vector<Behavior*> GetAllBehaviors_Internal(uint32_t entity_id, void* scene_ptr)
    {
        std::vector<Behavior*> result;
        ::Entity raw_e = { entity_id, static_cast<::Scene*>(scene_ptr) };
        
        ::ScriptComponent* script_comp = ::Entity_GetScripts(raw_e);
        if (script_comp)
        {
            for (uint32_t i = 0; i < script_comp->count; i++)
            {
                if (script_comp->instances[i].instance_data)
                {
                    result.push_back(static_cast<Behavior*>(script_comp->instances[i].instance_data));
                }
            }
        }
        
        return result;
    }
}