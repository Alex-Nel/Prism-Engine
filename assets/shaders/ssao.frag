#version 330 core
out float FragColor;
in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform int kernelSize = 16; // Default to 64, can be lowered for performance
uniform float radius = 0.15;  // How far the occlusion hemisphere reaches (TWEAK THIS)
uniform float bias = 0.025;  // Prevents self-shadowing acne on flat surfaces

uniform mat4 projection;     // To project the samples back to the screen
uniform vec2 noiseScale;     // ScreenRes / NoiseTextureRes (e.g. 1920/4.0, 1080/4.0)

void main()
{
    vec3 normal = texture(gNormal, TexCoords).xyz;
    
    // If the normal is exactly 0,0,0 (the clear color), this is the sky. Do not calculate SSAO.
    if (length(normal) < 0.1)
    {
        FragColor = 1.0; // Fully lit
        return;
    }
    
    normal = normalize(normal);
    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz);

    // Create the TBN matrix to orient the hemisphere along the surface normal
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // Get sample position in View Space
        vec3 samplePos = TBN * samples[i]; 
        samplePos = fragPos + samplePos * radius; 

        // Project sample position to screen space to look up the depth
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset; 
        offset.xyz /= offset.w; 
        offset.xyz = offset.xyz * 0.5 + 0.5; 

        // Get depth of the geometry at this screen coordinate
        float sampleDepth = texture(gPosition, offset.xy).z; 

        float distance = abs(fragPos.z - sampleDepth);
        float rangeCheck = smoothstep(radius, radius * 0.5, distance);
        
        // If the sampled depth is closer to the camera than our sample point, it is occluded!
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;           
    }
    
    // Invert the occlusion so 1.0 = Bright (Unoccluded) and 0.0 = Black (Occluded)
    occlusion = 1.0 - (occlusion / float(kernelSize));
    
    // Optional: Raise to a power to increase contrast
    FragColor = pow(occlusion, 1.2); 
}