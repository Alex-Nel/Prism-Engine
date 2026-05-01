#pragma once

#include "Entity.hpp"
#include "Behavior.hpp"
#include <unordered_map>
#include <string>
#include <functional>
#include <cJSON.h>

namespace Prism
{
    class ScriptRegistry
    {
    public:
        // Returns a Behavior* so we can deserialize it
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
        static void SpawnFromJSON(Prism::Entity e, const std::string& className, cJSON* json_data)
        {
            if (Factory.find(className) != Factory.end())
            {
                Behavior* new_script = Factory[className](e);
                
                // Immediately load the custom variables into the new script!
                if (new_script && json_data) {
                    new_script->Engine_Deserialize(json_data);
                }
            } else {
                Debug_Warn("Script class %s not found in Registry!", className.c_str());
            }
        }
    };
}



extern "C"
{
    // Extern function that scene.c can call
    void Bridge_SpawnScript(::Entity raw_e, const char* class_name, struct cJSON* json_data)
    {
        Prism::ScriptRegistry::SpawnFromJSON(Prism::Entity(raw_e), std::string(class_name), json_data);
    }
}