#version 330 core

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec3 WorldPos;
in vec2 TexCoords;
in mat3 TBN;

struct Material
{
    sampler2D albedoMap;
    sampler2D normalMap;
    sampler2D metallicMap;
    sampler2D roughnessMap;
    sampler2D aoMap;

    bool hasNormalMap;
    bool hasMetallicMap;
    bool hasRoughnessMap;
    bool hasAOMap;

    vec3 albedoTint;
    float metallicFactor;
    float roughnessFactor;
};

uniform Material u_Material;
uniform float u_ReceiveShadows;
uniform float u_Gamma;



void main()
{
    // --- Albedo ---
    vec4 texColor = texture(u_Material.albedoMap, TexCoords);
    if (texColor.a < 0.1)
        discard;

    float gamma = u_Gamma > 0.01 ? u_Gamma : 2.2;
    texColor.rgb = pow(texColor.rgb, vec3(gamma));
    
    // --- Normal Mapping ---
    vec3 finalNormal;
    if (u_Material.hasNormalMap)
    {
        vec3 normalMap = texture(u_Material.normalMap, TexCoords).rgb;
        normalMap = normalMap * 2.0 - 1.0;   
        finalNormal = normalize(TBN * normalMap); 
    }
    else
    {
        finalNormal = normalize(TBN[2]); // Fallback to geometry normal
    }

    // --- Metallic & Roughness ---
    float metallic = u_Material.hasMetallicMap ? texture(u_Material.metallicMap, TexCoords).b : u_Material.metallicFactor;
    float roughness = u_Material.hasRoughnessMap ? texture(u_Material.roughnessMap, TexCoords).g : u_Material.roughnessFactor;
    float ao = u_Material.hasAOMap ? texture(u_Material.aoMap, TexCoords).r : 1.0;

    gPosition = vec4(WorldPos, metallic);    
    gNormal = vec4(finalNormal, (u_ReceiveShadows > 0.5) ? ao : -ao);
    gAlbedoSpec.rgb = texColor.rgb * u_Material.albedoTint;
    gAlbedoSpec.a = roughness;
}