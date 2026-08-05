#version 330 core

out vec4 FragColor;

void main()
{
    // OpenGL automatically writes the depth value to the bound texture.
    FragColor = vec4(1.0);
}