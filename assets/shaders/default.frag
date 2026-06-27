#version 330 core


#define MAX_DIR_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8



out vec4 FragColor;

in vec2 TexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;
// in vec4 FragPosLightSpace;


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

// Comparison sampler: hardware does the depth test + bilinear PCF per tap.
uniform sampler2DShadow shadowMap;
uniform mat4 u_LightSpaceMatrix;   // light view-projection (set on this program by the renderer)
uniform float u_ShadowTexelSize;   // world-space size of one shadow-map texel



// ==========================================
// Lighting Calculations
// ==========================================

// Calculates if the pixel is in shadow (0.0 = bright, 1.0 = shadowed)
float ShadowCalculation(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    float NdotL = max(dot(normal, lightDir), 0.0);

    // Normal-offset bias: only push surfaces that face the light. Interior/back-facing
    // pixels get zero offset, which prevents pushing samples through roof/wall seams
    // and leaking light onto the top edges of interior geometry.
    float slope = clamp(1.0 - NdotL, 0.0, 1.0);
    vec3 offsetPos = fragPos + normal * (u_ShadowTexelSize * NdotL * (0.5 + 2.0 * slope));

    vec4 fragPosLightSpace = u_LightSpaceMatrix * vec4(offsetPos, 1.0);

    // Perspective divide (-1..1), then remap to [0,1] texture space
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Outside the shadow frustum: treat as lit (border color handles XY; this catches Z)
    if(projCoords.z > 1.0) return 0.0;

    // Slope-scaled depth bias in NDC space. Caster-side glPolygonOffset handles most
    // acne, so this can stay small and avoids pulling compare depth past roof geometry.
    float compareDepth = projCoords.z - max(0.0005 * slope, 0.0001);

    // --- Hardware PCF ---
    // shadowMap is a comparison sampler with GL_LINEAR, so each tap already does a
    // 2x2 bilinear depth comparison. Looping a 3x3 grid of taps yields a smooth,
    // effectively ~6x6 filtered penumbra instead of hard, blocky stair-stepped edges.
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            // texture() on a sampler2DShadow returns the fraction of samples that are LIT
            float lit = texture(shadowMap, vec3(projCoords.xy + vec2(x, y) * texelSize, compareDepth));
            shadow += 1.0 - lit;
        }    
    }
    shadow /= 9.0;
    

    return shadow;
}



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

    float shadow = ShadowCalculation(v_FragPos, normal, lightDir);
    
    return ( ambient + (1.0 - shadow) * (diffuse + specular) );
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
    for (int i = 0; i < MAX_DIR_LIGHTS; i++) {
        if (i >= u_DirLightCount) break;
        totalLight += CalcDirLight(u_DirLights[i], norm, viewDir);
    }

    // 2. Accumulate Point Lights
    for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        if (i >= u_PointLightCount) break;
        totalLight += CalcPointLight(u_PointLights[i], norm, v_FragPos, viewDir);
    }

    // 3. Accumulate Spot Lights
    for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
        if (i >= u_SpotLightCount) break;
        totalLight += CalcSpotLight(u_SpotLights[i], norm, v_FragPos, viewDir);
    }

    
    // Final Pixel Color
    vec4 texColor = texture(u_Material.diffuse, TexCoord);
    vec3 tintedAlbedo = texColor.rgb * u_Material.tint;
    
    // Multiply the accumulated physical light by the tinted surface color
    FragColor = vec4(totalLight * tintedAlbedo, texColor.a);
}