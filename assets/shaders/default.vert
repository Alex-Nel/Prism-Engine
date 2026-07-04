#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec2 TexCoord;
out vec3 v_Normal;
out vec3 v_FragPos;



void main()
{
   vec3 worldPos = vec3(u_Model * vec4(aPos, 1.0));
   
   v_FragPos = worldPos;

   mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
   v_Normal = normalMatrix * aNormal;

   // Pass the UVs straight through to the fragment shader
   TexCoord = aTexCoords;
   gl_Position = u_Projection * u_View * vec4(worldPos, 1.0);
}