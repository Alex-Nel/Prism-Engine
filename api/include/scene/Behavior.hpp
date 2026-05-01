#pragma once

#include "Entity.hpp"
#include <type_traits>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


// Forward decleration
struct cJSON;


namespace Prism
{
    // --- Base class for all custom behaviors ---
    class Behavior
    {
    private:
        using PropertyPtr = std::variant<int*, float*, bool*, std::string*>;
        std::unordered_map<std::string, PropertyPtr> m_Properties; // Maps the variables string name to its memory address

    protected:
        // This function will bind variables to the engine (used in a constructor)
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

        void Engine_Serialize(struct cJSON* json);
        void Engine_Deserialize(struct cJSON* json);
    };




    // ==========================================
    // Internal Helpers
    // ==========================================
    
    void BindBehavior_Internal(uint32_t entity_id, void* scene_ptr, Behavior* instance);
    std::vector<Behavior*> GetAllBehaviors_Internal(uint32_t entity_id, void* scene_ptr);





    // --- Adds a custom script to an entity ---
    template<typename T>
    inline T* Entity::AddScript()
    {
        static_assert(std::is_base_of<Behavior, T>::value, "T must inherit from Prism::Behavior");

        // Make a new instance of the script
        T* instance = new T();
        instance->entity = *this;

        BindBehavior_Internal(this->id, this->scene_ptr, instance);

        return instance;
    }



    // --- Retrieves a pointer to a specific script instance ---
    template<typename T>
    inline T* Entity::GetScript()
    {
        static_assert(std::is_base_of<Behavior, T>::value, "T must inherit from Prism::Behavior");

        // Returns a vector of Behaviors
        std::vector<Behavior*> all_scripts = GetAllBehaviors_Internal(this->id, this->scene_ptr);
        
        for (Behavior* base_behavior : all_scripts)
        {
            if (T* target_script = dynamic_cast<T*>(base_behavior))
            {
                return target_script;
            }
        }
        
        return nullptr;
    }

}