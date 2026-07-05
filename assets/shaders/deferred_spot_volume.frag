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

// --- Shadow Uniforms ---
uniform sampler2DArrayShadow spotShadowMap;
uniform mat4 u_LightSpaceMatrix;
uniform int u_ShadowIndex;



// Interleaved Gradient Noise for shadow dithering
float InterleavedGradientNoise(vec2 positionScreen) 
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(positionScreen, magic.xy)));
}



// Vogel Disk
vec2 VogelDiskSample(int sampleIndex, int samplesCount, float phi) 
{
    const float GoldenAngle = 2.39996323;
    float r = sqrt(float(sampleIndex) + 0.5) / sqrt(float(samplesCount));
    float theta = float(sampleIndex) * GoldenAngle + phi;
    return vec2(r * cos(theta), r * sin(theta));
}



// Calculates the spot shadow
float CalculateSpotShadow(vec3 fragPos, vec3 normal, vec3 lightDir)
{
    if (u_ShadowIndex < 0)
        return 0.0;

    // A standard perspective shadow bias
    float NdotL = max(dot(normal, lightDir), 0.0);
    float bias = max(0.005 * (1.0 - NdotL), 0.0005);
    
    vec4 fragPosLightSpace = u_LightSpaceMatrix * vec4(fragPos, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0)
        return 0.0;

    // Apply bias to the Z coordinate
    projCoords.z -= bias;

    float shadow = 0.0;
    vec2 texelSizeUV = 1.0 / vec2(textureSize(spotShadowMap, 0).xy);
    
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    float phi = noise * 6.28318530718;
    int NUM_SAMPLES = 16;
    float filterRadius = 2.0;

    for(int i = 0; i < 16; i++) 
    {
        vec2 offset = VogelDiskSample(i, NUM_SAMPLES, phi) * texelSizeUV * filterRadius;
        // Pass u_ShadowIndex as the array layer
        float lit = texture(spotShadowMap, vec4(projCoords.xy + offset, float(u_ShadowIndex), projCoords.z));
        shadow += 1.0 - lit;
    }
    
    return shadow / float(NUM_SAMPLES);
}





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

    float shadow = CalculateSpotShadow(fragPos, normal, lightDir);
    float receivesShadows = texture(gNormal, TexCoords).a;
    shadow *= receivesShadows;
    
    vec3 diffuse = diff * u_LightColor * u_Intensity;
    vec3 specular = specStrength * spec * u_LightColor * u_Intensity;
    
    vec3 finalLight = (1.0 - shadow) * (diffuse + specular) * attenuation * spotIntensity;
    FragColor = vec4(finalLight * albedo, 0.0);

    // Uncomment to test spot lights (should appear green)
    // FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}