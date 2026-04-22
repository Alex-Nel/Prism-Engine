#pragma once

// ==========================================
// Prism Engine - Scripting API
// ==========================================

// Core Systems
#include "core/Math.hpp"
#include "core/Time.hpp"
#include "core/Input.hpp"
#include "core/IO.hpp"
#include "core/Log.hpp"
#include "core/Mesh.hpp"
#include "core/Image.hpp"

// Assets & platform
#include "AssetManager.hpp"
#include "Platform.hpp"

// Scene & ECS
#include "scene/Components.hpp"
#include "scene/Entity.hpp"
#include "scene/Scene.hpp"
#include "scene/Behavior.hpp" 

// Engine Lifecycle
#include "Engine.hpp"



namespace Prism
{
    // --- Lifecycle Bridges ---
    inline void Bridge_OnStart(::Entity e, void* data) {
        if (data) static_cast<Behavior*>(data)->OnStart();
    }
    inline void Bridge_OnUpdate(::Entity e, float dt, void* data) {
        if (data) static_cast<Behavior*>(data)->OnUpdate(dt);
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
    T* AddScript(Entity e)
    {
        static_assert(std::is_base_of<Behaviour, T>::value, "T must inherit from Prism::Behavior");

        T* instance = new T();
        instance->entity = e; 

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

        ::Entity_BindScript(e.raw, script);

        return instance;
    }
}