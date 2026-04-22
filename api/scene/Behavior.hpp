#pragma once

#include "Entity.hpp"

namespace Prism
{
    class Behavior
    {
    public:
        // The entity this script is attached to.
        Entity entity; 

        // Virtual destructor ensures proper cleanup of derived classes
        virtual ~Behavior() = default;

        // --- Lifecycle Callbacks ---
        
        // Called once the first time the entity is updated
        virtual void OnStart() {}
        
        // Called every frame
        virtual void OnUpdate(float dt) {}
        
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

}