#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 WorldPos;
out vec3 WorldNormal;
out vec2 TexCoords;

void main()
{
    // Calculate World Position
    vec3 worldPos = vec3(u_Model * vec4(aPos, 1.0));
    WorldPos = worldPos;

    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    WorldNormal = normalMatrix * aNormal;

    TexCoords = aTexCoords;

    // Output gl_Position
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}