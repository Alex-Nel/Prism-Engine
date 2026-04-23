#pragma once

#include "Entity.hpp"
#include <type_traits>

namespace Prism
{
    class Behavior
    {
    public:
        // The entity this script is attached to.
        Prism::Entity entity; 

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



        // --- PHYSICS CALLBACKS ---
        
        // Triggers
        virtual void OnTriggerEnter(Entity other) {}
        virtual void OnTriggerStay(Entity other) {}
        virtual void OnTriggerExit(Entity other) {}

        // Solid Collisions
        virtual void OnCollisionEnter(Entity other) {}
        virtual void OnCollisionStay(Entity other) {}
        virtual void OnCollisionExit(Entity other) {}
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





    // --- AddScript Template ---
    template<typename T>
    inline T* Entity::AddScript()
    {
        static_assert(std::is_base_of<Behavior, T>::value, "T must inherit from Prism::Behavior");

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

        ::Entity_BindScript(this->raw, script);

        return instance;
    }

}