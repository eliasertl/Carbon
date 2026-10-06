#version 450

// The host's own content in VulkanMinimalIntegration: one colored triangle, without vertex buffers.

layout(location = 0) out vec3 outColor;

const vec2 Positions[3] = vec2[](vec2(0.0, -0.62), vec2(-0.68, 0.58), vec2(0.68, 0.58));
const vec3 Colors[3] = vec3[](vec3(1.0, 0.27, 0.23), vec3(0.19, 0.82, 0.35), vec3(0.04, 0.52, 1.0));

void main()
{
    gl_Position = vec4(Positions[gl_VertexIndex], 0.0, 1.0);
    outColor = Colors[gl_VertexIndex];
}
