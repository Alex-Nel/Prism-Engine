#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords; // The UV coordinates from your MeshData

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec2 TexCoord;
out vec3 v_Normal;
out vec3 v_FragPos;

void main()
{
   v_FragPos = vec3(u_Model * vec4(aPos, 1.0));
   v_Normal = mat3(u_Model) * aNormal;

   // Pass the UVs straight through to the fragment shader!
   TexCoord = aTexCoords;
   
   // gl_Position = u_Projection * u_View * u_Model * vec4(aPos, 1.0);
   gl_Position = u_Projection * u_View * vec4(v_FragPos, 1.0);
}