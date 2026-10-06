#version 450

// Vertex stage of the Vulkan backend. A port of VertexMain in Backends/WebGPU/Shaders/Carbon.wgsl; keep them in step.

layout(push_constant) uniform Frame
{
    // Size of the display area in points.
    vec2 DisplaySize;
    // Pixels per point.
    float ContentScale;
    // 1.0 when the render target has an sRGB format and expects linear color values.
    float LinearOutput;
} frame;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inLocal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in uint inPrimitive;

layout(location = 0) out vec2 outLocal;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec4 outColor;
layout(location = 3) flat out uint outPrimitive;

vec3 SrgbToLinear(vec3 color)
{
    vec3 low = color / 12.92;
    vec3 high = pow((color + vec3(0.055)) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(color, vec3(0.04045)));
}

void main()
{
    vec2 normalized = inPosition / frame.DisplaySize;
    // Vulkan's clip space has y pointing down, like Carbon's points.
    gl_Position = vec4(normalized.x * 2.0 - 1.0, normalized.y * 2.0 - 1.0, 0.0, 1.0);
    outLocal = inLocal;
    outUV = inUV;
    outPrimitive = inPrimitive;
    // Colors are authored in sRGB. Unorm targets take them as they are (blending in gamma space, as macOS UI
    // does); sRGB targets need linear values.
    outColor = vec4(mix(inColor.rgb, SrgbToLinear(inColor.rgb), frame.LinearOutput), inColor.a);
}
