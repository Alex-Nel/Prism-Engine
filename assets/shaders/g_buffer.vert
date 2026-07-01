#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 ViewPos;
out vec3 ViewNormal;

void main()
{
    // Calculate World Position
    vec3 worldPos = vec3(u_Model * vec4(aPos, 1.0));

    // Calculate View Space data for the G-Buffer
    vec4 viewSpacePos = u_View * vec4(worldPos, 1.0);
    ViewPos = viewSpacePos.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(u_View * u_Model)));
    ViewNormal = normalMatrix * aNormal;

    // Output gl_Position
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}