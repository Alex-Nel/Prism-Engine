#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D hdrLightingMap;
uniform float u_Gamma;



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
    vec4 lightingSample = texture(hdrLightingMap, TexCoords);
    if (lightingSample.a <= 0.0)
        discard;

    vec3 linearLight = lightingSample.rgb;
    vec3 mapped = ACESFilm(linearLight);
    float gamma = u_Gamma > 0.01 ? u_Gamma : 2.2;
    mapped = pow(mapped, vec3(1.0 / gamma));
    FragColor = vec4(mapped, 1.0);
}