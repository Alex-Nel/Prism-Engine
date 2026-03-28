#version 330 core

out vec4 FragColor;


in vec2 TexCoord;
in vec3 v_Normal;
in vec3 v_FragPos;

// This represents the texture bound in C.
uniform sampler2D u_Texture; 

uniform vec3 u_ViewPos;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_AmbientStrength;


void main()
{

    // 1. AMBIENT (Base light so shadows aren't pitch black)
    vec3 ambient = u_AmbientStrength * u_LightColor;
    
    // 2. DIFFUSE (Direct directional lighting)
    vec3 norm = normalize(v_Normal);
    // vec3 lightDir = normalize(u_LightDir - v_FragPos);
    vec3 lightDir = normalize(u_LightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;
    
    // 3. SPECULAR (The shiny reflection)
    float specularStrength = 0.5; // How shiny the material is
    vec3 viewDir = normalize(u_ViewPos - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    // 32 is the "shininess" factor (higher = tighter highlight)
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * u_LightColor;  
        
    // Combine everything with the texture color
    vec3 result = (ambient + diffuse + specular);
    vec4 texColor = texture(u_Texture, TexCoord);
    
    FragColor = vec4(result * texColor.rgb, texColor.a);
    // FragColor = vec4(norm * 0.5 + 0.5, 1.0);
}