#version 330 core

layout (location = 0) in vec3 aPos;
uniform mat4 u_Model;

void main()
{
    // Output world space position directly to the Geometry Shader
    gl_Position = u_Model * vec4(aPos, 1.0); 
}