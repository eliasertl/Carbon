// The host's own content in DX11Minimal: one colored triangle, without vertex buffers.

cbuffer Constants : register(b0)
{
    float Brightness;
};

struct PixelInput
{
    float4 Position : SV_Position;
    float3 Color : COLOR;
};

PixelInput VertexMain(uint index : SV_VertexID)
{
    const float2 positions[3] = {float2(0.0, 0.62), float2(0.68, -0.58), float2(-0.68, -0.58)};
    const float3 colors[3] = {float3(1.0, 0.27, 0.23), float3(0.04, 0.52, 1.0), float3(0.19, 0.82, 0.35)};
    PixelInput output;
    output.Position = float4(positions[index], 0.0, 1.0);
    output.Color = colors[index];
    return output;
}

float4 PixelMain(PixelInput input) : SV_Target
{
    return float4(input.Color * Brightness, 1.0);
}
