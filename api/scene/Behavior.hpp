#pragma once

#include "Entity.hpp"
#include "cJSON.h"
#include <type_traits>
#include <string>
#include <unordered_map>
#include <variant>



namespace Prism
{
    class Behavior
    {
    private:
        using PropertyPtr = std::variant<int*, float*, bool*, std::string*>;
        std::unordered_map<std::string, PropertyPtr> m_Properties; // Maps the variables string name to its memory address

    protected:
        // The user calls this in their constructor to "bind" a variable to the engine
        template<typename T>
        void ExposeVariable(const std::string& name, T* variable_ptr) {
            m_Properties[name] = variable_ptr;
        }

    public:
        // The entity this script is attached to.
        Prism::Entity entity; 
        std::string script_class_name;

        // Virtual destructor ensures proper cleanup of derived classes
        virtual ~Behavior() = default;

        // --- Lifecycle Callbacks ---
        
        // Called once the first time the entity is updated
        virtual void OnStart() {}
        
        // Called every frame
        virtual void OnUpdate() {}
        
        // Called when the entity or script is destroyed
        virtual void OnDestroy() {}
        
        // Called when the entity is set to active
        virtual void OnEnable() {}
        
        // Called when the entity is set to inactive
        virtual void OnDisable() {}



        // --- Physics Callbacks ---
        
        // Triggers
        virtual void OnTriggerEnter(Entity other) {}
        virtual void OnTriggerStay(Entity other) {}
        virtual void OnTriggerExit(Entity other) {}

        // Solid Collisions
        virtual void OnCollisionEnter(Entity other) {}
        virtual void OnCollisionStay(Entity other) {}
        virtual void OnCollisionExit(Entity other) {}



        // --- Automatic Serialization ---

        void Engine_Serialize(cJSON* json)
        {
            cJSON_AddStringToObject(json, "class_name", script_class_name.c_str());

            for (const auto& pair : m_Properties)
            {
                const std::string& name = pair.first;
                
                // Check what type of pointer we stored and save it appropriately
                if (std::holds_alternative<float*>(pair.second))
                    cJSON_AddNumberToObject(json, name.c_str(), *std::get<float*>(pair.second));
                else if (std::holds_alternative<int*>(pair.second))
                    cJSON_AddNumberToObject(json, name.c_str(), *std::get<int*>(pair.second));
                else if (std::holds_alternative<bool*>(pair.second))
                    cJSON_AddBoolToObject(json, name.c_str(), *std::get<bool*>(pair.second));
            }
        }

        void Engine_Deserialize(cJSON* json)
        {
            for (const auto& pair : m_Properties)
            {
                const std::string& name = pair.first;
                cJSON* item = cJSON_GetObjectItem(json, name.c_str());
                
                if (!item) continue; // Skip if it's not in the save file

                if (std::holds_alternative<float*>(pair.second) && cJSON_IsNumber(item))
                    *std::get<float*>(pair.second) = item->valuedouble;
                else if (std::holds_alternative<int*>(pair.second) && cJSON_IsNumber(item))
                    *std::get<int*>(pair.second) = item->valueint;
                else if (std::holds_alternative<bool*>(pair.second) && cJSON_IsBool(item))
                    *std::get<bool*>(pair.second) = cJSON_IsTrue(item);
            }
        }
    };





    // --- Lifecycle Bridges ---
    inline void Bridge_OnStart(::Entity e, void* data) {
        if (data) static_cast<Behavior*>(data)->OnStart();
    }
    inline void Bridge_OnUpdate(::Entity e, void* data) {
        if (data) static_cast<Behavior*>(data)->OnUpdate();
    }
    inline void Bridge_OnDestroy(::Entity e, void* data) { 
        if (data) { 
            Behavior* b = static_cast<Behavior*>(data);
            b->OnDestroy(); 
            delete b; 
        } 
    }
    inline void Bridge_OnEnable(::Entity e, void* data) {
        if (data) static_cast<Behavior*>(data)->OnEnable();
    }
    inline void Bridge_OnDisable(::Entity e, void* data) {
        if (data) static_cast<Behavior*>(data)->OnDisable();
    }



    // --- Collision Bridges ---

    inline void Bridge_OnTriggerEnter(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnTriggerEnter(Entity(other));
    }
    inline void Bridge_OnTriggerStay(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnTriggerStay(Entity(other));
    }
    inline void Bridge_OnTriggerExit(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnTriggerExit(Entity(other));
    }
    

    inline void Bridge_OnCollisionEnter(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnCollisionEnter(Entity(other));
    }
    inline void Bridge_OnCollisionStay(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnCollisionStay(Entity(other));
    }
    inline void Bridge_OnCollisionExit(::Entity self, ::Entity other, void* data) {
        if (data)
            static_cast<Behavior*>(data)->OnCollisionExit(Entity(other));
    }



    // --- Serialization Bridges ---

    inline void Bridge_OnSerialize(::Entity e, void* data, cJSON* json) {
        if (data) static_cast<Behavior*>(data)->Engine_Serialize(json);
    }
    inline void Bridge_OnDeserialize(::Entity e, void* data, cJSON* json) {
        if (data) static_cast<Behavior*>(data)->Engine_Deserialize(json);
    }





    // --- AddScript Template ---
    template<typename T>
    inline T* Entity::AddScript()
    {
        static_assert(std::is_base_of<Behavior, T>::value, "T must inherit from Prism::Behavior");

        // Make a new instance of the script
        T* instance = new T();
        instance->entity = *this;

        ::ScriptInstance script = {0};
        script.instance_data = instance;
        
        // Map all function bridges
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

        ::Entity_BindScript(this->raw, script);

        return instance;
    }



    // --- GetScript Template ---
    template<typename T>
    inline T* Entity::GetScript()
    {
        static_assert(std::is_base_of<Behavior, T>::value, "T must inherit from Prism::Behavior");

        // Grab the script component array from the backend
        ::ScriptComponent* script_comp = ::Entity_GetScripts(this->raw);
        if (!script_comp) return nullptr;

        // Loop through every script attached to this entity
        for (uint32_t i = 0; i < script_comp->count; i++)
        {
            // Cast the void* back to the base class
            Behavior* base_behavior = static_cast<Behavior*>(script_comp->instances[i].instance_data);
            
            // dynamic_cast checks if base_behavior is actually of type T, and returns pointer if it is
            if (base_behavior)
            {
                T* target_script = dynamic_cast<T*>(base_behavior);
                if (target_script)
                    return target_script;
            }
        }

        // Return nullptr if script was not found
        return nullptr;
    }

}