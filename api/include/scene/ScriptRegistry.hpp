#pragma once

#include "Entity.hpp"
#include "Behavior.hpp"
#include <unordered_map>
#include <string>
#include <functional>
#include "../PrismAPI.hpp"


// Forward decleration
struct cJSON;


namespace Prism
{
    // --- Class for the script registory ---

    class PRISM_API ScriptRegistry
    {
    public:
        // Returns a Behavior* so it can be deserialized
        using Instantiator = std::function<Behavior*(Prism::Entity)>;
        static inline std::unordered_map<std::string, Instantiator> Factory;

        template<typename T>
        static void Register(const std::string& className)
        {
            Factory[className] = [className](Prism::Entity e) -> Behavior* {
                T* script = e.AddScript<T>();
                script->script_class_name = className;
                return script;
            };
        }

        // Spawns the script AND loads its variables from the JSON
        static void SpawnFromJSON(Prism::Entity e, const std::string& className, struct cJSON* json_data);
    };
}