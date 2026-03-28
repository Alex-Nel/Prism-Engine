#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;

// --- GLOBAL UNIFORMS ---
uniform sampler2D u_Texture; 
uniform vec3 u_ViewPos;

// --- DIRECTIONAL LIGHT (The Sun) ---
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_AmbientStrength;

// --- POINT LIGHTS ---
struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

#define MAX_POINT_LIGHTS 4
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_PointLightCount;


void main()
{
    // Pre-calculate vectors we will reuse
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    float specularStrength = 0.5; // Base shininess for the material
    float shininess = 32.0;

    // ==========================================
    // 1. DIRECTIONAL LIGHT (The Sun)
    // ==========================================
    vec3 ambient = u_AmbientStrength * u_LightColor;
    
    vec3 dirLightDir = normalize(u_LightDir);
    float diff = max(dot(norm, dirLightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;
    
    vec3 reflectDir = reflect(-dirLightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * u_LightColor;  
        
    // Start our total light accumulator with the Sun
    vec3 totalLight = (ambient + diffuse + specular);


    // ==========================================
    // 2. POINT LIGHTS (The Glowing Orbs)
    // ==========================================
    for(int i = 0; i < u_PointLightCount; i++) {
        
        PointLight light = u_PointLights[i];
        
        // A. Distance & Attenuation (Inverse Square Law Falloff)
        float distance = length(light.position - v_FragPos);
        float attenuation = 1.0 / (light.constant + 
                                   light.linear * distance + 
                                   light.quadratic * (distance * distance));    
        
        // B. Diffuse
        // Note: We DO subtract FragPos here because point lights actually have a physical location!
        vec3 ptLightDir = normalize(light.position - v_FragPos);
        float ptDiff = max(dot(norm, ptLightDir), 0.0);
        vec3 ptDiffuse = ptDiff * light.color * light.intensity;
        
        // C. Specular
        vec3 ptReflectDir = reflect(-ptLightDir, norm);  
        float ptSpec = pow(max(dot(viewDir, ptReflectDir), 0.0), shininess);
        vec3 ptSpecular = specularStrength * ptSpec * light.color * light.intensity;  
        
        // Add this light's power to the total, weakened by the distance falloff
        totalLight += (ptDiffuse + ptSpecular) * attenuation;
    }


    // ==========================================
    // 3. FINAL PIXEL COLOR
    // ==========================================
    vec4 texColor = texture(u_Texture, TexCoord);
    
    // Multiply the accumulated light by the raw texture colors
    FragColor = vec4(totalLight * texColor.rgb, texColor.a);
}