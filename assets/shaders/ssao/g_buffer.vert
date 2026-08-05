#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 WorldPos;
out vec2 TexCoords;
out mat3 TBN;

void main()
{
    // Calculate World Position
    vec3 worldPos = vec3(u_Model * vec4(aPos, 1.0));
    WorldPos = worldPos;
    TexCoords = aTexCoords;

    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 N = normalize(normalMatrix * aNormal);

    // Re-orthogonalize T with respect to N (Gram-Schmidt process)
    vec3 T = normalMatrix * aTangent;
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
    
    // The bitangent is the cross product of the normal and the tangent
    vec3 B = cross(N, T);
    
    TBN = mat3(T, B, N);

    // Output gl_Position
    gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}