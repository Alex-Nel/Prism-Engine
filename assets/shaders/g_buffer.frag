#version 330 core
layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;

in vec3 ViewPos;
in vec3 ViewNormal;

void main()
{    
    // Store the view-space position
    gPosition = vec4(ViewPos, 1.0);
    
    // Store the view-space normals (must be normalized!)
    gNormal = vec4(normalize(ViewNormal), 1.0);
}