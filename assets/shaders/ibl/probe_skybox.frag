#version 330 core

out vec4 FragColor;
in vec3 TexCoords;

uniform samplerCube u_Skybox;
uniform bool u_IsHDR;

void main()
{
    vec3 sampleCoords = u_IsHDR ? TexCoords : vec3(-TexCoords.x, TexCoords.y, TexCoords.z);
    vec3 color = texture(u_Skybox, sampleCoords).rgb;
    if (!u_IsHDR)
        color = pow(color, vec3(2.2));

    FragColor = vec4(color, 1.0);
}