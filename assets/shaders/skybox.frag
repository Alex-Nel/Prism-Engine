#version 330 core

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube u_Skybox;

void main()
{
    vec3 flippedCoords = vec3(-TexCoords.x, TexCoords.y, TexCoords.z);

    FragColor = texture(u_Skybox, flippedCoords);
}