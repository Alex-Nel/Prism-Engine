#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;

// --- GLOBAL UNIFORMS ---
uniform vec3 u_ViewPos;

// --- MATERIAL STRUCT ---
struct Material {
    sampler2D diffuse; 
    vec3 tint;
    float shininess;
    float specularStrength;
};
uniform Material u_Material;

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

#define MAX_POINT_LIGHTS 8
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_PointLightCount;


void main()
{
    // Pre-calculate vectors we will reuse
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);

    // ==========================================
    // 1. DIRECTIONAL LIGHT (The Sun)
    // ==========================================
    vec3 ambient = u_AmbientStrength * u_LightColor;
    
    vec3 dirLightDir = normalize(u_LightDir);
    float diff = max(dot(norm, dirLightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;
    
    vec3 reflectDir = reflect(-dirLightDir, norm);  
    
    // NEW: Use the material's physical properties!
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    vec3 specular = u_Material.specularStrength * spec * u_LightColor;  
        
    vec3 totalLight = (ambient + diffuse + specular);

    // ==========================================
    // 2. POINT LIGHTS (The Glowing Orbs)
    // ==========================================
    for(int i = 0; i < u_PointLightCount; i++) {
        
        PointLight light = u_PointLights[i];
        
        float distance = length(light.position - v_FragPos);
        float attenuation = 1.0 / (light.constant + 
                                   light.linear * distance + 
                                   light.quadratic * (distance * distance));    
        
        vec3 ptLightDir = normalize(light.position - v_FragPos);
        float ptDiff = max(dot(norm, ptLightDir), 0.0);
        vec3 ptDiffuse = ptDiff * light.color * light.intensity;
        
        vec3 ptReflectDir = reflect(-ptLightDir, norm);  
        
        // NEW: Use the material's physical properties here too!
        float ptSpec = pow(max(dot(viewDir, ptReflectDir), 0.0), u_Material.shininess);
        vec3 ptSpecular = u_Material.specularStrength * ptSpec * light.color * light.intensity;  
        
        totalLight += (ptDiffuse + ptSpecular) * attenuation;
    }

    // ==========================================
    // 3. FINAL PIXEL COLOR
    // ==========================================
    // Sample the texture using the material's sampler
    vec4 texColor = texture(u_Material.diffuse, TexCoord);
    
    // Apply the material's tint color to the raw texture
    vec3 tintedAlbedo = texColor.rgb * u_Material.tint;
    
    // Multiply the accumulated physical light by the tinted surface color
    FragColor = vec4(totalLight * tintedAlbedo, texColor.a);
}