#version 330 core

uniform sampler2D Tex;
uniform vec2 scaleUV;
uniform vec2 offsetUV;

uniform float chromaKeyFactor;
uniform vec3 chromaKeyColor;
uniform float opacity;

in vec2 UV;
out vec4 fragColor;

vec3 rgb2hsv(vec3 rgb)
{
	float Cmax = max(rgb.r, max(rgb.g, rgb.b));
	float Cmin = min(rgb.r, min(rgb.g, rgb.b));
    float delta = Cmax - Cmin;

	vec3 hsv = vec3(0., 0., Cmax);
	
	if (Cmax > Cmin)
	{
		hsv.y = delta / Cmax;

		if (rgb.r == Cmax)
			hsv.x = (rgb.g - rgb.b) / delta;
		else
		{
			if (rgb.g == Cmax)
				hsv.x = 2. + (rgb.b - rgb.r) / delta;
			else
				hsv.x = 4. + (rgb.r - rgb.g) / delta;
		}
		hsv.x = fract(hsv.x / 6.);
	}
	return hsv;
}

float chromaKey(vec3 color)
{
	vec3 weights = vec3(chromaKeyFactor, 1., 2.);
	vec3 hsv = rgb2hsv(color);
	vec3 target = rgb2hsv(chromaKeyColor);
	float dist = length(weights * (target - hsv));
	return 1. - clamp(3. * dist - 1.5, 0., 1.);
}

void main()
{
    vec4 color = texture(Tex, (UV.st * scaleUV) + offsetUV);
	
	float incrustation = chromaKey(color.rgb);
	
	color = mix(color, vec4(0.0), incrustation);
	
	color.a *= opacity;

	fragColor = color;
}
