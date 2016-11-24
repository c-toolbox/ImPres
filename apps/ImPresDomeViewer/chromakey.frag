#version 330 core

uniform sampler2D Tex;
uniform vec2 scaleUV;
uniform vec2 offsetUV;

uniform vec3 chromaKeyColor;
uniform float chromaKeyCutOff;
uniform float chromaKeyThresholdSensitivity;
uniform float chromaKeySmoothing;

in vec2 UV;
out vec4 color;

void main()
{
    vec4 texColor = texture(Tex, (UV.st * scaleUV) + offsetUV);
     
    float maskY = 0.2989 * chromaKeyColor.r + 0.5866 * chromaKeyColor.g + 0.1145 * chromaKeyColor.b;
    float maskCr = 0.7132 * (chromaKeyColor.r - maskY);
    float maskCb = 0.5647 * (chromaKeyColor.b - maskY);
     
    float Y = 0.2989 * texColor.r + 0.5866 * texColor.g + 0.1145 * texColor.b;
    float Cr = 0.7132 * (texColor.r - Y);
    float Cb = 0.5647 * (texColor.b - Y);
     
    float blendValue = smoothstep(chromaKeyThresholdSensitivity, chromaKeyThresholdSensitivity + chromaKeySmoothing, distance(vec2(Cr, Cb), vec2(maskCr, maskCb)));
    if(blendValue > chromaKeyCutOff)
        color = vec4(texColor.rgb, texColor.a * blendValue);
    else
        discard;
}