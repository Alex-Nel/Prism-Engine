#version 330 core

layout (location = 0) in vec3 aPos;
// Support for animations
layout (location = 3) in ivec4 aBoneIds;
layout (location = 4) in vec4 aWeights;

const int MAX_BONES = 128;
uniform mat4 u_BoneMatrices[MAX_BONES];
uniform mat4 u_Model;



void main()
{
    vec4 totalPosition = vec4(0.0);
    bool hasBones = false;
    for (int i = 0 ; i < 4 ; i++)
    {
        if(aBoneIds[i] == -1)
            continue;

        hasBones = true;
        totalPosition += (u_BoneMatrices[aBoneIds[i]] * vec4(aPos, 1.0)) * aWeights[i];
    }

    if (!hasBones)
        totalPosition = vec4(aPos, 1.0);
    
    // Output world space position directly to the Geometry Shader
    gl_Position = u_Model * totalPosition; 
}