#version 330 core

out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform vec3 u_ViewPos;

// --- Single Spot Light Uniforms ---
uniform vec2 u_ScreenSize;
uniform vec3 u_LightPos;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_Intensity;
uniform float u_Constant;
uniform float u_Linear;
uniform float u_Quadratic;
uniform float u_Radius;
uniform float u_CutOff;
uniform float u_OuterCutOff;

// --- Shadow Uniforms ---
uniform sampler2DArrayShadow spotShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform int u_ShadowIndex;



// Interleaved Gradient Noise for shadow dithering
float InterleavedGradientNoise(vec2 positionScreen) 
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(positionScreen, magic.xy)));
}



// Vogel Disk
vec2 VogelDiskSample(int sampleIndex, int samplesCount, float phi) 
{
    const float GoldenAngle = 2.39996323;
    float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * GoldenAngle + phi;
    return vec2(r * cos(theta), r * sin(theta));
}



// Calculates the spot shadow
float CalculateSpotShadow(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    if (u_ShadowIndex < 0)
        return 0.0;

    // A standard perspective shadow bias
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 offsetPos = fragPos + normal * (0.05 * (1.0 - NdotL) + 0.01);
    
    vec4 fragPosLightSpace = u_LightSpaceMatrix * vec4(offsetPos, 1.0);
    if (fragPosLightSpace.w <= 0.0)
        return 0.0;

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z < 0.0 || projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // Apply bias to the Z coordinate
    projCoords.z -= 0.0005;

    float shadow = 0.0;
    vec2 texelSizeUV = 1.0 / vec2(textureSize(spotShadowMap, 0).xy);
    
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float phi = noise * 6.28318530718;
    int NUM_SAMPLES = 16;
    float filterRadius = 2.0;

    for(int i = 0; i < 16; i++) 
    {
        vec2 offset = VogelDiskSample(i, NUM_SAMPLES, phi) * texelSizeUV * filterRadius;
        // Pass u_ShadowIndex as the array layer
        float lit = texture(spotShadowMap, vec4(projCoords.xy + offset, float(u_ShadowIndex), projCoords.z));
        shadow += 1.0 - lit;
    }
    
    return shadow / float(NUM_SAMPLES);
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



// Geometry Schlick-GGX - How much do the micro-facets shadow each other?
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










void main()
{
    vec2 TexCoords = gl_FragCoord.xy / u_ScreenSize;

    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 albedo = texture(gAlbedoSpec, TexCoords).rgb;
    
    float metallic = texture(gPosition, TexCoords).a;
    float roughness = texture(gAlbedoSpec, TexCoords).a;

    if (length(normal) < 0.1)
        discard;

    float distance = length(u_LightPos - fragPos);
    if (distance > u_Radius)
        discard;

    float radiusFade = 1.0 - smoothstep(u_Radius * 0.8, u_Radius, distance);
    
    vec3 lightDir = normalize(u_LightPos - fragPos);

    // Spotlight Soft Edges Intensity (angular smoothstep using true physical cone angles in radians)
    float theta = dot(lightDir, normalize(-u_LightDir));
    float spotIntensity = smoothstep(u_OuterCutOff, u_CutOff, theta);
    if (spotIntensity <= 0.0)
        discard;

    vec3 viewDir = normalize(u_ViewPos - fragPos);
    vec3 halfVector = normalize(lightDir + viewDir);

    // Calculate attenuation
    float attenuation = 1.0 / (u_Constant + u_Linear * distance + u_Quadratic * (distance * distance));    
    vec3 radiance = u_LightColor * u_Intensity * attenuation * spotIntensity * radiusFade;

    // F0 represents the base reflectivity of the surface
    // Non-metals use 0.04. Metals use their albedo color
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    // --- PBR Cook-Torrance BRDF ---
    float NDF = DistributionGGX(normal, halfVector, roughness);   
    float G   = GeometrySmith(normal, viewDir, lightDir, roughness);      
    vec3 F    = FresnelSchlick(max(dot(halfVector, viewDir), 0.0), F0);
       
    vec3 numerator    = NDF * G * F; 
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, lightDir), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;

    // Energy conservation: Light reflected (F) + Light refracted (kD) = 1.0
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic; // Pure metals do not have diffuse light

    // Final Lighting Math
    float NdotL = max(dot(normal, lightDir), 0.0);

    float shadow = CalculateSpotShadow(fragPos, normal, lightDir);
    float receivesShadows = (texture(gNormal, TexCoords).a >= 0.0) ? 1.0 : 0.0;
    shadow *= receivesShadows;
    
    // Outgoing Light (Lo)
    vec3 Lo = (kD * albedo + specular) * radiance * NdotL;
    
    vec3 finalLight = (1.0 - shadow) * Lo;

    FragColor = vec4(finalLight, 0.0);

    // Uncomment to test spot lights (should appear green)
    // FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}