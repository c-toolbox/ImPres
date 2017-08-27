#version 330 core

uniform sampler2D Tex;
uniform vec2 scaleUV;
uniform vec2 offsetUV;
uniform float opacity;

in vec2 UV;
out vec4 color;

void main()
{
    color = texture(Tex, (UV.st * scaleUV) + offsetUV);
	color.a *= opacity;
}