#version 330 core

layout (location = 0) in vec3 aPos;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec4 ClipSpacePos;



void main()
{
    // We pass the Clip Space position to the fragment shader
    ClipSpacePos = u_Projection * u_View * u_Model * vec4(aPos, 1.0);
    gl_Position = ClipSpacePos;
}