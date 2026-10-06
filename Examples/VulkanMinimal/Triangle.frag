#version 450

layout(push_constant) uniform Constants
{
    float Brightness;
} constants;

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = vec4(inColor * constants.Brightness, 1.0);
}
