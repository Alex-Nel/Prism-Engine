#version 330 core

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube u_Skybox;
uniform float u_Gamma;
uniform bool u_IsHDR;



// ACES approximation
vec3 ACESFilm(vec3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}



void main()
{
    vec3 flippedCoords = vec3(-TexCoords.x, TexCoords.y, TexCoords.z);
    vec3 color = texture(u_Skybox, flippedCoords).rgb;

    // FragColor = texture(u_Skybox, flippedCoords);
    if (u_IsHDR)
    {
        // HDR textures are linear, so tone-map them to display range
        color = ACESFilm(color);
    }
    else
    {
        // LDR textures (like png skyboxes) are already in sRGB space. We linearize them first so they react accurately to custom gamma.
        color = pow(color, vec3(2.2));
    }

    // Apply the user's custom gamma correction
    float gamma = u_Gamma > 0.01 ? u_Gamma : 2.2;
    color = pow(color, vec3(1.0 / gamma));

    FragColor = vec4(color, 1.0);
}