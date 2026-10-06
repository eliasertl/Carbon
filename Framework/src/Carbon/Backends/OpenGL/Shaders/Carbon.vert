#version 330 core

// Vertex stage of the OpenGL backend. A port of VertexMain in Backends/WebGPU/Shaders/Carbon.wgsl; keep them in step.

// Display size in points, pixels per point, and 1.0 when the target expects linear color values.
uniform vec4 Frame;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inLocal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in uint inPrimitive;

out vec2 vLocal;
out vec2 vUV;
out vec4 vColor;
flat out uint vPrimitive;

vec3 SrgbToLinear(vec3 color)
{
    vec3 low = color / 12.92;
    vec3 high = pow((color + vec3(0.055)) / 1.055, vec3(2.4));
    return mix(high, low, lessThanEqual(color, vec3(0.04045)));
}

void main()
{
    vec2 normalized = inPosition / Frame.xy;
    // OpenGL's clip space has y pointing up; Carbon's points point down.
    gl_Position = vec4(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0, 0.0, 1.0);
    vLocal = inLocal;
    vUV = inUV;
    vPrimitive = inPrimitive;
    // Colors are authored in sRGB. Unorm targets take them as they are (blending in gamma space, as macOS UI
    // does); sRGB targets need linear values.
    vColor = vec4(mix(inColor.rgb, SrgbToLinear(inColor.rgb), Frame.w), inColor.a);
}
