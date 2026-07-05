#version 330 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out; // We output 3 vertices * 6 faces = 18 vertices total per input triangle

uniform mat4 u_ShadowMatrices[6];
out vec4 FragPos; // Passed to the fragment shader



void main()
{
    // Clone the triangle onto all 6 faces of the cubemap
    for(int face = 0; face < 6; ++face)
    {
        // Tell OpenGL exactly which layer in our Texture Array to write to
        gl_Layer = face; 
        
        for(int i = 0; i < 3; ++i)
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = u_ShadowMatrices[face] * FragPos;
            EmitVertex();
        }

        EndPrimitive();
    }
}