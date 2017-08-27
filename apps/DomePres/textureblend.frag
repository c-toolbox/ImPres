#version 330 core

uniform sampler2D Tex0;
uniform sampler2D Tex1;
uniform float texMix;
uniform vec2 scaleUV;
uniform vec2 offsetUV;

in vec2 UV;
out vec4 color;

void main()
{
    color = mix(texture(Tex0, (UV.st * scaleUV) + offsetUV), texture(Tex1, (UV.st * scaleUV) + offsetUV), texMix);
}