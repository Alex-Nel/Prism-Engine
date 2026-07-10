#version 330 core

out vec4 FragColor;
in vec4 ClipSpacePos;

// --- G-Buffer ---
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 u_ViewPos;

// --- Single Point Light Uniforms ---
uniform vec2 u_ScreenSize;
uniform vec3 u_LightPos;
uniform vec3 u_LightColor;
uniform float u_Intensity;
uniform float u_Constant;
uniform float u_Linear;
uniform float u_Quadratic;
uniform float u_Radius;

uniform samplerCubeShadow pointShadowMap;
uniform int u_ShadowIndex;
uniform float u_FarPlane;



// Adds noise to a gradient
float InterleavedGradientNoise(vec2 positionScreen) 
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(positionScreen, magic.xy)));
}



// A 20-tap 3D distribution for omnidirectional PCF
vec3 sampleOffsetDirections[20] = vec3[] (
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);



// Calculates the shadow for a point light
float CalculatePointShadow(vec3 fragPos)
{
    if (u_ShadowIndex < 0)
        return 0.0;

    vec3 fragToLight = fragPos - u_LightPos;
    float currentDepth = length(fragToLight);
    
    float shadow = 0.0;
    float bias = 0.07;
    int samples = 20;
    float viewDistance = length(u_ViewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / u_FarPlane)) / 25.0;

    // Add a tiny bit of noise to the radius to shatter the bands
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    diskRadius *= (0.8 + 0.4 * noise);

    // The depth value we want the hardware to compare against (0.0 to 1.0)
    float compareDepth = (currentDepth - bias) / u_FarPlane;
    if (compareDepth > 1.0 || compareDepth < 0.0)
        return 0.0;
    
    for(int i = 0; i < samples; ++i)
    {
        // Standard samplerCube only requires the 3D direction vector
        vec3 sampleDir = fragToLight + sampleOffsetDirections[i] * diskRadius;

        // samplerCubeShadow takes a vec4(direction.x, direction.y, direction.z, compareDepth)
        // It returns an interpolated blend instead of either 0.0 or 1.0
        float lit = texture(pointShadowMap, vec4(sampleDir, compareDepth));
        shadow += 1.0 - lit;
    }

    return shadow / float(samples);
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
    // Convert the 3D Sphere position into a 2D Screen UV coordinate (Perspective Divide)
    vec2 TexCoords = gl_FragCoord.xy / u_ScreenSize;

    // Read G-Buffer
    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 albedo = texture(gAlbedoSpec, TexCoords).rgb;
    
    float metallic = texture(gPosition, TexCoords).a;
    float roughness = texture(gAlbedoSpec, TexCoords).a;

    if (length(normal) < 0.1)
        discard; // Ignore skybox

    // Distance Cull - Because depth testing is disabled the sphere draws over everything.
    // If the geometry on screen is physically outside the light's radius, ignore it
    float distance = length(u_LightPos - fragPos);
    if (distance > u_Radius)
        discard;

    // Calculate Lighting
    vec3 viewDir = normalize(u_ViewPos - fragPos);
    vec3 lightDir = normalize(u_LightPos - fragPos);
    vec3 halfVector = normalize(lightDir + viewDir);

    // Calculate attenuation
    float attenuation = 1.0 / (u_Constant + u_Linear * distance + u_Quadratic * (distance * distance));    
    vec3 radiance = u_LightColor * u_Intensity * attenuation;

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

    float shadow = CalculatePointShadow(fragPos);
    float receivesShadows = texture(gNormal, TexCoords).a;
    shadow *= receivesShadows;
    
    // Outgoing Light (Lo)
    vec3 Lo = (kD * albedo + specular) * radiance * NdotL;
    
    vec3 finalLight = (1.0 - shadow) * Lo;

    // --- HDR Tone Mapping (ACES Filmic) & Gamma ---
    // ACES approximation:
    finalLight = clamp((finalLight * (2.51f * finalLight + 0.03f)) / (finalLight * (2.43f * finalLight + 0.59f) + 0.14f), 0.0, 1.0);
    finalLight = pow(finalLight, vec3(1.0 / 2.2));

    FragColor = vec4(finalLight, 0.0);
}