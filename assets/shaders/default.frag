#version 330 core


#define MAX_DIR_LIGHTS 4
#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 8
#define MAX_SHADOW_CASCADES 4



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
#define MAX_SHADOW_CASCADES 4


uniform DirLight u_DirLights[MAX_DIR_LIGHTS];
uniform int u_DirLightCount;

uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_PointLightCount;

uniform SpotLight u_SpotLights[MAX_SPOT_LIGHTS];
uniform int u_SpotLightCount;

// Comparison sampler: hardware does the depth test + bilinear PCF per tap.
uniform sampler2DArrayShadow shadowMap;
uniform mat4 u_LightSpaceMatrices[MAX_SHADOW_CASCADES];
uniform float u_ShadowTexelSizes[MAX_SHADOW_CASCADES];
uniform float u_CascadeSplits[MAX_SHADOW_CASCADES - 1];
uniform int u_ShadowCascadeCount;
uniform mat4 u_View;
uniform float u_ShadowCameraNear;
uniform float u_CascadeBlendFraction;
uniform sampler2D ssaoMap;
uniform bool u_EnableSSAO;
uniform float u_ShadowMaxDistance;



// ==========================================
// Lighting Calculations
// ==========================================

float GetViewDepth(vec3 fragPos)
{
    return -(u_View * vec4(fragPos, 1.0)).z;
}



// Selects a shadow cascade from a fragment position
int SelectShadowCascade(float viewDepth)
{
    if (u_ShadowCascadeCount <= 1)
        return 0;

    for (int i = 0; i < MAX_SHADOW_CASCADES - 1; ++i)
    {
        if (i >= u_ShadowCascadeCount - 1)
            break;
        if (viewDepth < u_CascadeSplits[i])
            return i;
    }

    return u_ShadowCascadeCount - 1;
}



// True when fragPos projects inside this cascade's shadow-map XY frustum (before bias).
bool CascadeCoversFragment(int cascade, vec3 fragPos)
{
    vec4 lightSpace = u_LightSpaceMatrices[cascade] * vec4(fragPos, 1.0);
    vec3 coords = lightSpace.xyz / lightSpace.w;
    coords = coords * 0.5 + 0.5;
    return coords.x >= 0.0 && coords.x <= 1.0 && coords.y >= 0.0 && coords.y <= 1.0 && coords.z <= 1.0;
}



// Calculates if the pixel is in shadow (0.0 = bright, 1.0 = shadowed)
float SampleCascadeShadow(int cascade, vec3 fragPos, vec3 normal, vec3 lightDir)
{
    if (!CascadeCoversFragment(cascade, fragPos))
        return 0.0;

    float texelSize = u_ShadowTexelSizes[cascade];
    float NdotL = max(dot(normal, lightDir), 0.0);
    float offsetScale = clamp(1.0 - NdotL, 0.0, 1.0);

    // Calculate how far to push the sample out along the normal
    float calculatedOffset = texelSize * offsetScale * 2.0;

    // Never let the offset exceed 0.05 meters, preventing it from piercing thin geometry!
    calculatedOffset = min(calculatedOffset, 0.05);

    vec3 offsetPos = fragPos + normal * calculatedOffset;


    vec4 fragPosLightSpace = u_LightSpaceMatrices[cascade] * vec4(offsetPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    
    float shadow = 0.0;
    vec2 texelSizeUV = 1.0 / vec2(textureSize(shadowMap, 0).xy);
    
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec2 sampleUV = projCoords.xy + vec2(x, y) * texelSizeUV;
            
            // Skip samples that fall completely outside the cascade UV bounds
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
                continue;

            float lit = texture(shadowMap, vec4(sampleUV, float(cascade), projCoords.z));
            shadow += 1.0 - lit;
        }
    }
    
    return shadow / 9.0;
}



// Calculates if the pixel is in shadow (0.0 = bright, 1.0 = shadowed)
float ShadowCalculation(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    if (u_ShadowCascadeCount <= 1)
        return SampleCascadeShadow(0, fragPos, normal, lightDir);

    float viewDepth = GetViewDepth(fragPos);
    int cascade = SelectShadowCascade(viewDepth);
    float shadow = SampleCascadeShadow(cascade, fragPos, normal, lightDir);

    if (cascade >= u_ShadowCascadeCount - 1)
        return shadow;

    float splitFar = u_CascadeSplits[cascade];
    float splitNear = (cascade == 0) ? u_ShadowCameraNear : u_CascadeSplits[cascade - 1];
    float sliceDepth = splitFar - splitNear;
    float blendRange = sliceDepth * u_CascadeBlendFraction;
    float blendStart = splitFar - blendRange;

    if (viewDepth > blendStart && CascadeCoversFragment(cascade + 1, fragPos))
    {
        float t = smoothstep(blendStart, splitFar, viewDepth);
        float nextShadow = SampleCascadeShadow(cascade + 1, fragPos, normal, lightDir);
        shadow = mix(shadow, nextShadow, t);
    }


    // Fade shadows out in the distance
    float distanceToCam = length(u_ViewPos - fragPos);
    float fadeStart = max(u_ShadowMaxDistance - 30.0, 0.0); // Start fading 30 units before the edge
    
    if (distanceToCam > fadeStart) 
    {
        float fadeFactor = clamp((distanceToCam - fadeStart) / 30.0, 0.0, 1.0);
        shadow = mix(shadow, 0.0, fadeFactor); // Blend shadow to 0.0 (fully lit)
    }


    return shadow;
}





// ==========================================
// Lighting Calculations
// ==========================================

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float ao)
{
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), u_Material.shininess);
    
    // Combine
    vec3 ambient = light.ambientStrength * light.color * light.intensity * ao;
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

    float ambientOcclusion = 1.0; 
    if (u_EnableSSAO) {
        vec2 screenUV = gl_FragCoord.xy / vec2(textureSize(ssaoMap, 0));
        ambientOcclusion = texture(ssaoMap, screenUV).r;
    }

    vec3 totalLight = vec3(0.0);


    // Accumulate directional lights
    for (int i = 0; i < MAX_DIR_LIGHTS; i++) {
        if (i >= u_DirLightCount) break;
        totalLight += CalcDirLight(u_DirLights[i], norm, viewDir, ambientOcclusion);
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