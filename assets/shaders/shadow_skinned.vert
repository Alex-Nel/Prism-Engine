#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 3) in vec3 aTangent;

// Animation inputs (same attribute slots as animated.vert)
layout (location = 4) in ivec4 aBoneIds;
layout (location = 5) in vec4 aWeights;

uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;


const int MAX_BONES = 128;
uniform mat4 u_BoneMatrices[MAX_BONES];

void main()
{
    // Skin the vertex into its animated pose so the shadow matches the visible mesh
    vec4 totalPosition = vec4(0.0);
    for (int i = 0; i < 4; i++)
    {
        if (aBoneIds[i] < 0 || aBoneIds[i] >= MAX_BONES)
            continue;

        totalPosition += (u_BoneMatrices[aBoneIds[i]] * vec4(aPos, 1.0)) * aWeights[i];
    }

    // Fallback for sub-meshes with no bone weights: use the raw local position.
    if (totalPosition.w == 0.0)
        totalPosition = vec4(aPos, 1.0);

    gl_Position = u_LightSpaceMatrix * u_Model * totalPosition;
}