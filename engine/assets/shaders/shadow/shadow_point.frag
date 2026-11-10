#version 330 core

in vec4 FragPos;

uniform vec3 u_LightPos;
uniform float u_FarPlane;



void main()
{
    // We calculate the physical 3D distance between the pixel and the light
    float lightDistance = length(FragPos.xyz - u_LightPos);
    
    // Map it to a linear [0.0, 1.0] range
    lightDistance = lightDistance / u_FarPlane;
    
    // Write this linear depth directly into the depth map
    gl_FragDepth = lightDistance;
}