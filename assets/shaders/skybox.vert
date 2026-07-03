#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 u_Projection;
uniform mat4 u_View;

void main()
{
    TexCoords = aPos;

    // Strip the translation (position) from the view matrix
    mat4 viewNoTranslation = mat4(mat3(u_View)); 
    vec4 pos = u_Projection * viewNoTranslation * vec4(aPos, 1.0);
    
    // The Z=W trick pushes the depth to exactly 1.0
    gl_Position = pos.xyww; 
}