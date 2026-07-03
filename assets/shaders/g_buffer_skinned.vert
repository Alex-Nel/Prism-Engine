#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in ivec4 aBoneIds;
layout (location = 4) in vec4 aWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

const int MAX_BONES = 128;
uniform mat4 u_BoneMatrices[MAX_BONES];

out vec3 WorldPos;
out vec3 WorldNormal;
out vec2 TexCoords;

void main()
{
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);

    for(int i = 0 ; i < 4 ; i++)
    {
        if(aBoneIds[i] == -1)
            continue;

        vec4 localPosition = u_BoneMatrices[aBoneIds[i]] * vec4(aPos, 1.0);
        totalPosition += localPosition * aWeights[i];

        mat3 boneNormalMatrix = mat3(u_BoneMatrices[aBoneIds[i]]);
        totalNormal += boneNormalMatrix * aNormal * aWeights[i];
    }

    if (totalPosition.w == 0.0)
    {
        totalPosition = vec4(aPos, 1.0);
        totalNormal = aNormal;
    }

    // Calculate World Position
    vec3 worldPos = vec3(u_Model * totalPosition);
    WorldPos = worldPos;

    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    WorldNormal = normalMatrix * totalNormal;

    TexCoords = aTexCoords;

    // Output identical gl_Position
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}