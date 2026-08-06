#version 330 core



out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssaoMap;
uniform bool u_EnableSSAO;

uniform bool u_HasGlobalIBL;
uniform samplerCube globalIrradianceMap;
uniform samplerCube globalPrefilterMap;
uniform samplerCube localIrradianceMap;
uniform samplerCube localPrefilterMap;
uniform sampler2D brdfLUT;
uniform int u_IBLDebugMode;

uniform vec3 u_ViewPos;
uniform vec2 u_ScreenSize;
uniform vec3 u_GlobalAmbientColor;
uniform float u_GlobalAmbientIllumination;
uniform vec3 u_ProbePosition;
uniform vec3 u_ProbeBoxMin;
uniform vec3 u_ProbeBoxMax;
uniform float u_ProbeBlendDistance;





vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}





vec3 BoxProjectDirection(vec3 direction, vec3 position)
{
    vec3 safeDirection = direction;
    safeDirection.x = abs(safeDirection.x) < 0.0001 ? (safeDirection.x < 0.0 ? -0.0001 : 0.0001) : safeDirection.x;
    safeDirection.y = abs(safeDirection.y) < 0.0001 ? (safeDirection.y < 0.0 ? -0.0001 : 0.0001) : safeDirection.y;
    safeDirection.z = abs(safeDirection.z) < 0.0001 ? (safeDirection.z < 0.0 ? -0.0001 : 0.0001) : safeDirection.z;

    vec3 tToMin = (u_ProbeBoxMin - position) / safeDirection;
    vec3 tToMax = (u_ProbeBoxMax - position) / safeDirection;
    vec3 tFar = max(tToMin, tToMax);
    float distanceToWall = min(min(tFar.x, tFar.y), tFar.z);
    vec3 hitPosition = position + safeDirection * max(distanceToWall, 0.0);

    return normalize(hitPosition - u_ProbePosition);
}





vec3 EvaluateIBL(samplerCube irradianceSampler, samplerCube prefilterSampler, vec3 normal, vec3 reflectionDirection, vec3 viewDirection, vec3 albedo, float metallic, float roughness, float ao)
{
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float NdotV = max(dot(normal, viewDirection), 0.0);
    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 diffuse = texture(irradianceSampler, normal).rgb * albedo;
    vec3 prefiltered = textureLod(prefilterSampler, reflectionDirection, roughness * 4.0).rgb;
    vec2 brdf = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specular = prefiltered * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * ao * u_GlobalAmbientIllumination;
}





vec3 CompressIrradiance(vec3 irradiance)
{
    if (u_IBLDebugMode == 1)
        return irradiance / (irradiance + vec3(1.0));

    float luminance = dot(irradiance, vec3(0.2126, 0.7152, 0.0722));
    float compressed = clamp(log2(1.0 + luminance) / log2(17.0), 0.0, 1.0);
    return vec3(compressed);
}





void main()
{
    vec2 screenUV = gl_FragCoord.xy / u_ScreenSize;
    vec3 fragPos = texture(gPosition, screenUV).xyz;
    vec3 sampledNormal = texture(gNormal, screenUV).xyz;

    if (length(sampledNormal) < 0.1 || any(lessThan(fragPos, u_ProbeBoxMin)) || any(greaterThan(fragPos, u_ProbeBoxMax)))
        discard;

    vec3 normal = normalize(sampledNormal);
    vec3 albedo = texture(gAlbedoSpec, screenUV).rgb;

    float metallic = texture(gPosition, screenUV).a;
    float roughness = texture(gAlbedoSpec, screenUV).a;
    float ao = abs(texture(gNormal, screenUV).a);
    
    if (u_EnableSSAO)
        ao *= texture(ssaoMap, screenUV).r;

    vec3 distanceToFace = min(fragPos - u_ProbeBoxMin, u_ProbeBoxMax - fragPos);
    float interiorDistance = min(min(distanceToFace.x, distanceToFace.y), distanceToFace.z);
    float weight = u_ProbeBlendDistance > 0.0001
        ? smoothstep(0.0, u_ProbeBlendDistance, interiorDistance)
        : 1.0;

    if (u_IBLDebugMode != 0)
    {
        vec3 localDebug = CompressIrradiance(texture(localIrradianceMap, normal).rgb);
        vec3 globalDebug = u_HasGlobalIBL
            ? CompressIrradiance(texture(globalIrradianceMap, normal).rgb)
            : vec3(0.0);
        FragColor = vec4((localDebug - globalDebug) * weight, 0.0);
        return;
    }

    vec3 viewDirection = normalize(u_ViewPos - fragPos);
    vec3 reflectionDirection = reflect(-viewDirection, normal);
    vec3 localReflectionDirection = BoxProjectDirection(reflectionDirection, fragPos);

    vec3 localIBL = EvaluateIBL(localIrradianceMap, localPrefilterMap, normal, localReflectionDirection, viewDirection, albedo, metallic, roughness, ao);

    vec3 previousAmbient = u_GlobalAmbientColor * u_GlobalAmbientIllumination * albedo * ao;
    if (u_HasGlobalIBL)
    {
        previousAmbient = EvaluateIBL(globalIrradianceMap, globalPrefilterMap, normal, reflectionDirection, viewDirection, albedo, metallic, roughness, ao);
    }

    FragColor = vec4((localIBL - previousAmbient) * weight, 0.0);
}
