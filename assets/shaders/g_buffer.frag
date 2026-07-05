#version 330 core

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec3 WorldPos;
in vec3 WorldNormal;
in vec2 TexCoords;

struct Material
{
    sampler2D diffuse;
    vec3 tint;
    float specularStrength;
};

uniform Material u_Material;
uniform float u_ReceiveShadows;



void main()
{    
    gPosition = vec4(WorldPos, 1.0);    
    gNormal = vec4(normalize(WorldNormal), u_ReceiveShadows);
    
    // Albedo (RGB) and Specular (Alpha)
    vec4 texColor = texture(u_Material.diffuse, TexCoords);
    
    // Discard completely transparent pixels so they don't block the background
    if (texColor.a < 0.1)
        discard; 
    
    gAlbedoSpec.rgb = texColor.rgb * u_Material.tint;
    gAlbedoSpec.a = u_Material.specularStrength;
}