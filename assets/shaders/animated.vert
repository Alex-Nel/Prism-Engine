#version 330 core

// Standard Inputs
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

// --- Animation Inputs ---
layout (location = 3) in ivec4 aBoneIds;
layout (location = 4) in vec4 aWeights;

// Camera & Object Matrices
uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

// --- Bone Matrices ---
const int MAX_BONES = 128;
uniform mat4 u_BoneMatrices[MAX_BONES];

// Outputs to Fragment Shader
out vec2 TexCoord;
out vec3 v_Normal;
out vec3 v_FragPos;



void main()
{
    // Initialize empty accumulators
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);

    // Loop through the 4 possible bones pulling on this vertex
    for(int i = 0 ; i < 4 ; i++)
    {
        // -1 means no bone is assigned to this slot
        if(aBoneIds[i] == -1) 
            continue;

        mat4 boneMatrix = u_BoneMatrices[aBoneIds[i]];
        float weight = aWeights[i];

        // 1. Deform the Position
        vec4 localPosition = boneMatrix * vec4(aPos, 1.0);
        totalPosition += localPosition * weight;

        // 2. Deform the Normal (mat3 strips the translation, keeping only rotation/scale)
        vec3 localNormal = mat3(boneMatrix) * aNormal;
        totalNormal += localNormal * weight;
    }

    // Safety net: If a sub-mesh has 0 bones, default back to its raw local position.
    if (totalPosition.w == 0.0) 
    {
        totalPosition = vec4(aPos, 1.0);
        totalNormal = aNormal;
    }

    // Pass the UVs straight through
    TexCoord = aTexCoords;

    vec3 worldPos = vec3(u_Model * totalPosition);
    v_FragPos = worldPos;

    // Calculate final World Position and World Normal for lighting
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    v_Normal = normalMatrix * totalNormal;

    // Calculate final Screen Position
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}