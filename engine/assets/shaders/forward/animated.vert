#version 330 core

// Standard Inputs
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

// --- Animation Inputs ---
layout (location = 4) in ivec4 aBoneIds;
layout (location = 5) in vec4 aWeights;

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
out mat3 v_TBN;



void main()
{
    // Initialize empty accumulators
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);
    vec3 totalTangent = vec3(0.0);

    // Loop through the 4 possible bones pulling on this vertex
    for(int i = 0 ; i < 4 ; i++)
    {
        // -1 means no bone is assigned to this slot
        if(aBoneIds[i] < 0 || aBoneIds[i] >= MAX_BONES) 
            continue;

        mat4 boneMatrix = u_BoneMatrices[aBoneIds[i]];
        float weight = aWeights[i];

        // Deform the Position
        vec4 localPosition = boneMatrix * vec4(aPos, 1.0);
        totalPosition += localPosition * weight;

        // Deform the Normal (mat3 strips the translation, keeping only rotation/scale)
        vec3 localNormal = mat3(boneMatrix) * aNormal;
        totalNormal += localNormal * weight;

        // Deform the tangent
        vec3 localTangent = mat3(boneMatrix) * aTangent;
        totalTangent += localTangent * weight;
    }

    // If a sub-mesh has 0 bones, default back to its raw local position.
    if (totalPosition.w == 0.0) 
    {
        totalPosition = vec4(aPos, 1.0);
        totalNormal = aNormal;
        totalTangent = aTangent;
    }

    // Pass the UVs straight through
    TexCoord = aTexCoords;

    vec3 worldPos = vec3(u_Model * totalPosition);
    v_FragPos = worldPos;

    // Calculate final World Position and World Normal for lighting
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 N = normalize(normalMatrix * totalNormal);
    vec3 T = normalMatrix * totalTangent;
    
    if (dot(T, T) > 1e-6)
    {
        T = normalize(T - dot(T, N) * N);
    }
    else
    {
        vec3 c1 = cross(N, vec3(0.0, 0.0, 1.0));
        vec3 c2 = cross(N, vec3(0.0, 1.0, 0.0));
        T = normalize(dot(c1, c1) > dot(c2, c2) ? c1 : c2);
    }

    vec3 B = cross(N, T);
    v_Normal = N;
    v_TBN = mat3(T, B, N);
    

    // Calculate final Screen Position
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}