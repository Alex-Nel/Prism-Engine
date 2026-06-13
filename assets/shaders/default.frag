#version 330 core

out vec4 FragColor;

in vec2 TexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;


// --- Global Uniforms ---

uniform vec3 u_ViewPos;



// --- Material Struct ---

struct Material {
    sampler2D diffuse; 
    vec3 tint;
    float shininess;
    float specularStrength;
};
uniform Material u_Material;



// ==========================================
// Lighting Structures
// ==========================================

struct DirLight {
    vec3 direction;
    vec3 color;
    float intensity;
    float ambientStrength;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    vec3 color;
    float intensity;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
};



// --- Arrays & Counts ---

#define MAX_DIR_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8


uniform DirLight u_DirLights[MAX_DIR_LIGHTS];
uniform int u_DirLightCount;

uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_PointLightCount;

uniform SpotLight u_SpotLights[MAX_SPOT_LIGHTS];
uniform int u_SpotLightCount;



// ==========================================
// Lighting Calculations
// ==========================================

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    
    // Combine
    vec3 ambient = light.ambientStrength * light.color * light.intensity;
    vec3 diffuse = diff * light.color * light.intensity;
    vec3 specular = u_Material.specularStrength * spec * light.color * light.intensity;
    
    return (ambient + diffuse + specular);
}



vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // Combine
    vec3 diffuse = diff * light.color * light.intensity;
    vec3 specular = u_Material.specularStrength * spec * light.color * light.intensity;
    
    return (diffuse + specular) * attenuation;
}



vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // Spotlight Soft Edges Intensity
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = light.cutOff - light.outerCutOff;
    float spotIntensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // Combine
    vec3 diffuse = diff * light.color * light.intensity;
    vec3 specular = u_Material.specularStrength * spec * light.color * light.intensity;
    
    return (diffuse + specular) * attenuation * spotIntensity;
}





void main()
{
    // Pre-calculate vectors we will reuse
    vec3 norm = normalize(v_Normal);
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);

    vec3 totalLight = vec3(0.0);


    // Accumulate directional lights
    for (int i = 0; i < u_DirLightCount; i++)
        totalLight += CalcDirLight(u_DirLights[i], norm, viewDir);

    // 2. Accumulate Point Lights
    for (int i = 0; i < u_PointLightCount; i++)
        totalLight += CalcPointLight(u_PointLights[i], norm, v_FragPos, viewDir);

    // 3. Accumulate Spot Lights
    for (int i = 0; i < u_SpotLightCount; i++)
        totalLight += CalcSpotLight(u_SpotLights[i], norm, v_FragPos, viewDir);

    
    // Final Pixel Color
    vec4 texColor = texture(u_Material.diffuse, TexCoord);
    vec3 tintedAlbedo = texColor.rgb * u_Material.tint;
    
    // Multiply the accumulated physical light by the tinted surface color
    FragColor = vec4(totalLight * tintedAlbedo, texColor.a);
}