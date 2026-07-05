#version 330 core

out vec4 FragColor;
in vec4 ClipSpacePos;

// --- G-Buffer ---
uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

uniform vec3 u_ViewPos;

// --- Single Point Light Uniforms ---
uniform vec2 u_ScreenSize;
uniform vec3 u_LightPos;
uniform vec3 u_LightColor;
uniform float u_Intensity;
uniform float u_Constant;
uniform float u_Linear;
uniform float u_Quadratic;
uniform float u_Radius;

uniform samplerCubeShadow pointShadowMap;
uniform float u_FarPlane;



// Adds noise to a gradient
float InterleavedGradientNoise(vec2 positionScreen) 
{
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(positionScreen, magic.xy)));
}



// A 20-tap 3D distribution for omnidirectional PCF
vec3 sampleOffsetDirections[20] = vec3[] (
   vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
   vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
   vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
   vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
   vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
);



// Calculates the shadow for a point light
float CalculatePointShadow(vec3 fragPos)
{
    vec3 fragToLight = fragPos - u_LightPos;
    float currentDepth = length(fragToLight);
    
    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;
    float viewDistance = length(u_ViewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / u_FarPlane)) / 25.0;

    // Add a tiny bit of noise to the radius to shatter the bands
    float noise = InterleavedGradientNoise(gl_FragCoord.xy);
    diskRadius *= (0.8 + 0.4 * noise);

    // The depth value we want the hardware to compare against (0.0 to 1.0)
    float compareDepth = (currentDepth - bias) / u_FarPlane;
    
    for(int i = 0; i < samples; ++i)
    {
        // Standard samplerCube only requires the 3D direction vector
        vec3 sampleDir = fragToLight + sampleOffsetDirections[i] * diskRadius;

        // samplerCubeShadow takes a vec4(direction.x, direction.y, direction.z, compareDepth)
        // It returns an interpolated blend instead of either 0.0 or 1.0
        float lit = texture(pointShadowMap, vec4(sampleDir, compareDepth));
        shadow += 1.0 - lit;
    }

    return shadow / float(samples);
}



void main()
{
    // Convert the 3D Sphere position into a 2D Screen UV coordinate (Perspective Divide)
    vec2 TexCoords = gl_FragCoord.xy / u_ScreenSize;

    // Read G-Buffer
    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal = texture(gNormal, TexCoords).xyz;
    vec3 albedo = texture(gAlbedoSpec, TexCoords).rgb;
    float specStrength = texture(gAlbedoSpec, TexCoords).a;

    if (length(normal) < 0.1)
        discard; // Ignore skybox

    // Distance Cull - Because depth testing is disabled the sphere draws over everything.
    // If the geometry on screen is physically outside the light's radius, ignore it
    float distance = length(u_LightPos - fragPos);
    if (distance > u_Radius)
        discard;

    // Calculate Lighting
    vec3 lightDir = normalize(u_LightPos - fragPos);
    vec3 viewDir = normalize(u_ViewPos - fragPos);
    
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    float attenuation = 1.0 / (u_Constant + u_Linear * distance + u_Quadratic * (distance * distance));    

    float shadow = CalculatePointShadow(fragPos);
    
    vec3 diffuse = diff * u_LightColor * u_Intensity;
    vec3 specular = specStrength * spec * u_LightColor * u_Intensity;
    
    vec3 finalLight = (1.0 - shadow) * (diffuse + specular) * attenuation;
    
    FragColor = vec4(finalLight * albedo, 0.0);
}