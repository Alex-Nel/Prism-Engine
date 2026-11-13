#version 330 core

precision mediump float;
uniform sampler2D Texture;
uniform bool u_SDF;

in vec2 Frag_UV;
in vec4 Frag_Color;

out vec4 Out_Color;

const float SDF_EDGE = 128.0 / 255.0;   // Must match FONT_SDF_ONEDGE in font_core.h



void main()
{
   vec4 texel = texture(Texture, Frag_UV.st);

   if (u_SDF)
   {
      // Derivatives keep the antialiased edge about one screen pixel wide at any text size
      float edgeWidth = max(length(vec2(dFdx(texel.a), dFdy(texel.a))) * 0.70710678, 0.0001);
      float coverage = smoothstep(SDF_EDGE - edgeWidth, SDF_EDGE + edgeWidth, texel.a);
      Out_Color = vec4(Frag_Color.rgb * texel.rgb, Frag_Color.a * coverage);
      return;
   }

   Out_Color = Frag_Color * texel;
}