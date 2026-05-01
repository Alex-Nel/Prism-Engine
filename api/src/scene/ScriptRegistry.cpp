#include "../include/scene/ScriptRegistry.hpp"
#include "../include/core/Log.hpp"

#include <cJSON.h>


extern "C"
{
    #include "../../../scene/scene.h"
}


namespace Prism
{
    void ScriptRegistry::SpawnFromJSON(Prism::Entity e, const std::string& className, cJSON* json_data)
    {
        if (Factory.find(className) != Factory.end())
        {
            Behavior* new_script = Factory[className](e);
            
            if (new_script && json_data) {
                new_script->Engine_Deserialize(json_data);
            }
        }
        else
        {
            Debug_Warn("Script class %s not found in Registry!", className.c_str());
        }
    }
}



// ==========================================
// Backend Hook
// ==========================================

extern "C"
{
    // Used by the backend
    
    void Bridge_SpawnScript(::Entity raw_e, const char* class_name, struct cJSON* json_data)
    {
        Prism::ScriptRegistry::SpawnFromJSON(Prism::Entity(raw_e.id, raw_e.scene), std::string(class_name), json_data);
    }
}