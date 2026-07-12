#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

// --- G-Buffer Inputs ---
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssaoMap;
uniform bool u_EnableSSAO;

// --- Global Uniforms ---
uniform vec3 u_ViewPos;

// --- Arrays & Counts ---
#define MAX_DIR_LIGHTS 8
#define MAX_SHADOW_CASCADES 4

// Comparison sampler: hardware does the depth test + bilinear PCF per tap.
uniform sampler2DArrayShadow shadowMap;
uniform mat4 u_LightSpaceMatrices[MAX_SHADOW_CASCADES];
uniform float u_ShadowTexelSizes[MAX_SHADOW_CASCADES];
uniform float u_CascadeSplits[MAX_SHADOW_CASCADES - 1];
uniform int u_ShadowCascadeCount;
uniform mat4 u_View;
uniform float u_ShadowCameraNear;
uniform float u_CascadeBlendFraction;
uniform float u_ShadowMaxDistance;



// ==========================================
// Lighting Structures
// ==========================================

struct DirLight
{
    vec3 direction;
    vec3 color;
    float intensity;
    float ambientStrength;
    int castsShadows;
};



uniform DirLight u_DirLights[MAX_DIR_LIGHTS];
uniform int u_DirLightCount;
uniform float u_Gamma;
uniform vec3 u_GlobalAmbientColor;
uniform float u_GlobalAmbientIllumination;











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



// Interleaved Gradient Noise (Produces cinematic dither instead of stripes)
float InterleavedGradientNoise(vec2 positionScreen) 
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(positionScreen, magic.xy)));
}



// Vogel Disk (Perfect spiral distribution for blurring)
vec2 VogelDiskSample(int sampleIndex, int samplesCount, float phi) 
{
    const float GoldenAngle = 2.39996323;
    float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * GoldenAngle + phi;
    return vec2(r * cos(theta), r * sin(theta));
}



// Calculates if the pixel is in shadow (0.0 = bright, 1.0 = shadowed)
float SampleCascadeShadow(int cascade, vec3 fragPos, vec3 normal, vec3 lightDir)
{
    if (!CascadeCoversFragment(cascade, fragPos))
        return 0.0;

    float texelSize = u_ShadowTexelSizes[cascade];
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // Angle-Based Softness & Gentle Cascade Scaling
    // If the sun is hitting at a grazing angle (NdotL is close to 0), the shadow pixels 
    // are stretched massively. We dynamically increase the blur to hide the blockiness
    float grazingMultiplier = pow(1.0 - NdotL, 2.0); 
    float angleSoftness = 1.0 + (grazingMultiplier * 4.0);
    
    // Scale blur radius with cascade index so higher cascades get a wider filter kernel to eliminate blocky texel stair-steps
    float cascadeSoftness = 1.0 + float(cascade) * 0.35;
    float baseBlurRadius = 2.0 * cascadeSoftness;
    float finalRadius = baseBlurRadius * angleSoftness;

    // Using true geometric slope (tan theta) instead of linear 1-NdotL prevents stripe acne in Cascade 0.
    float slope = sqrt(max(1.0 - NdotL * NdotL, 0.0)) / max(NdotL, 0.05);
    float offsetScale = clamp(slope, 0.2, 2.0);
    float calculatedOffset = texelSize * offsetScale * finalRadius * 0.85;
    
    calculatedOffset = min(calculatedOffset, 1.0); 

    vec3 offsetPos = fragPos + normal * calculatedOffset;

    vec4 fragPosLightSpace = u_LightSpaceMatrices[cascade] * vec4(offsetPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0;

    vec2 texelSizeUV = 1.0 / vec2(textureSize(shadowMap, 0).xy);

    // Generate seamless noise based on screen pixel
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float phi = noise * 6.28318530718; // 2 * PI

    float shadow = 0.0;
    int NUM_SAMPLES = 16;
    
    // 16-Tap Vogel Disk Blur
    for(int i = 0; i < 16; i++) 
    {
        vec2 offset = VogelDiskSample(i, NUM_SAMPLES, phi) * texelSizeUV * finalRadius;
        
        vec2 sampleUV = projCoords.xy + offset;
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0)
            continue;

        float lit = texture(shadowMap, vec4(sampleUV, float(cascade), projCoords.z));
        shadow += 1.0 - lit;
    }
    
    return shadow / float(NUM_SAMPLES);
}



// Calculates if the pixel is in shadow (0.0 = bright, 1.0 = shadowed)
float ShadowCalculation(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    float receivesShadows = texture(gNormal, TexCoords).a;
    if (receivesShadows < 0.0) 
        return 0.0;

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

    shadow *= receivesShadows;

    return shadow;
}










// ==========================================
// PBR Math (Cook-Torrance BRDF)
// ==========================================

const float PI = 3.14159265359;

// Normal Distribution (GGX) - How many micro-facets point exactly at the light?
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.0000001); // Prevent divide by zero
}



// Geometry Schlick-GGX - How much do the micro-facets shadow each other
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 0.0000001);
}



float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}



// Fresnel Schlick - Reflection intensity based on viewing angle
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}










// PBR Directional Light calculation:
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float ao, vec3 albedo, float metallic, float roughness, vec3 fragPos)
{
    vec3 lightDir = normalize(-light.direction);
    vec3 halfVector = normalize(lightDir + viewDir);

    // Radiance of a directional light is just its color * intensity (no distance attenuation)
    vec3 radiance = light.color * light.intensity;

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    float NDF = DistributionGGX(normal, halfVector, roughness);   
    float G   = GeometrySmith(normal, viewDir, lightDir, roughness);      
    vec3 F    = FresnelSchlick(max(dot(halfVector, viewDir), 0.0), F0);
       
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; 

    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 Lo = (kD * albedo + specular) * radiance * NdotL;

    // Ambient lighting (scaled proportionally for PBR sRGB workflow so 0.1 strength produces rich, realistic shadows instead of flat gray interiors)
    vec3 ambient = light.color * (light.ambientStrength * 0.30) * albedo * ao;

    float shadow = 0.0;
    if (light.castsShadows == 1)
        shadow = ShadowCalculation(fragPos, normal, lightDir);
        
    return ambient + (1.0 - shadow) * Lo;
}










// ACES Filmic Tone Mapping approximation (preserves rich diffuse saturation and deep shadow contrast without washing out colors)
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}





void main()
{
    vec2 TexCoords = gl_FragCoord.xy / vec2(textureSize(gPosition, 0));

    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 albedo = texture(gAlbedoSpec, TexCoords).rgb;

    float metallic = texture(gPosition, TexCoords).a;
    float roughness = texture(gAlbedoSpec, TexCoords).a;

    if (length(normal) < 0.1) discard;

    float ao = abs(texture(gNormal, TexCoords).a);
    if (u_EnableSSAO)
        ao *= texture(ssaoMap, TexCoords).r;

    vec3 viewDir = normalize(u_ViewPos - fragPos);
    vec3 globalAmbient = u_GlobalAmbientColor * u_GlobalAmbientIllumination * albedo * ao;
    vec3 totalLight = globalAmbient;

    for (int i = 0; i < MAX_DIR_LIGHTS; i++)
    {
        if (i >= u_DirLightCount)
            break;
        totalLight += CalcDirLight(u_DirLights[i], normal, viewDir, ao, albedo, metallic, roughness, fragPos);
    }

    // --- HDR Tone Mapping (ACES Filmic) ---
    totalLight = ACESFilm(totalLight);

    // --- Gamma Correction ---
    // Converts linear physical light to sRGB space
    float gamma = u_Gamma > 0.01 ? u_Gamma : 2.2;
    totalLight = pow(totalLight, vec3(1.0 / gamma));


    FragColor = vec4(totalLight, 1.0);


    // --- Debugging the fragment shader, uncomment one to see the image ---

    // 1. View the raw Color/Albedo map (No shadows, no lights, just pure texture)
    // FragColor = vec4(albedo, 1.0); 

    // 2. View the World-Space Normal map (Looks like a rainbow neon world)
    // FragColor = vec4(normal, 1.0);

    // 3. View the Specular map (Grayscale)
    // FragColor = vec4(vec3(specStrength), 1.0);
}