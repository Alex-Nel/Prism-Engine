#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec2 TexCoord;
out vec3 v_Normal;
out vec3 v_FragPos;
out mat3 v_TBN;



void main()
{
   vec3 worldPos = vec3(u_Model * vec4(aPos, 1.0));
   
   v_FragPos = worldPos;

   mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
   vec3 N = normalize(normalMatrix * aNormal);
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

   vec3 B = cross(N, T);
   v_Normal = N;
   v_TBN = mat3(T, B, N);

   // Pass the UVs straight through to the fragment shader
   TexCoord = aTexCoords;
   gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}