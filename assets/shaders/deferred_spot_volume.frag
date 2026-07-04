#version 330 core

out vec4 FragColor;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform vec3 u_ViewPos;

// --- Single Spot Light Uniforms ---
uniform vec2 u_ScreenSize;
uniform vec3 u_LightPos;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_Intensity;
uniform float u_Constant;
uniform float u_Linear;
uniform float u_Quadratic;
uniform float u_Radius;
uniform float u_CutOff;
uniform float u_OuterCutOff;



void main()
{
    vec2 TexCoords = gl_FragCoord.xy / u_ScreenSize;

    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float specStrength = texture(gAlbedoSpec, TexCoords).a;

    if (length(normal) < 0.1)
        discard;

    float distance = length(u_LightPos - fragPos);
    if (distance > u_Radius)
        discard;

    vec3 lightDir = normalize(u_LightPos - fragPos);
    vec3 viewDir = normalize(u_ViewPos - fragPos);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    float attenuation = 1.0 / (u_Constant + u_Linear * distance + u_Quadratic * (distance * distance));    
    
    // Spotlight Cone Math
    float theta = dot(lightDir, normalize(-u_LightDir)); 
    float epsilon = u_CutOff - u_OuterCutOff;
    float spotIntensity = clamp((theta - u_OuterCutOff) / epsilon, 0.0, 1.0);
    
    vec3 diffuse = diff * u_LightColor * u_Intensity;
    vec3 specular = specStrength * spec * u_LightColor * u_Intensity;
    
    vec3 finalLight = (diffuse + specular) * attenuation * spotIntensity;
    FragColor = vec4(finalLight * albedo, 1.0);

    // Uncomment to test spot lights (should appear green)
    // FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}