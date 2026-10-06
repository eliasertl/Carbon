// The Direct3D 11 backend's shaders, shader model 4.0 (feature level 10.0 and later). A port of
// Backends/WebGPU/Shaders/Carbon.wgsl; the squircle functions mirror Framework/src/Carbon/Draw/Squircle.cpp. Change
// them together. fxc compiles VertexMain and PixelMain at build time.

cbuffer Frame : register(b0)
{
    // Size of the display area in points.
    float2 DisplaySize;
    // Pixels per point.
    float ContentScale;
    // 1.0 when the render target has an sRGB format and expects linear color values.
    float LinearOutput;
};

// The frame's primitives, two texels each: (half size, radius, smoothing) and (stroke width, softness, kind,
// reserved). Floats are stored as their bits, so an integer format returns them exactly.
Buffer<uint4> Primitives : register(t1);
Texture2D ColorTexture : register(t0);
SamplerState ColorSampler : register(s0);

static const uint KindSquircle = 0;
static const uint KindSquircleStroke = 1;
static const uint KindShadow = 2;
static const uint KindGlyph = 3;
static const uint KindImage = 4;

struct VertexInput
{
    float2 Position : POSITION;
    float2 Local : LOCAL;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
    uint Primitive : PRIMITIVE;
};

struct PixelInput
{
    float4 Position : SV_Position;
    float2 Local : LOCAL;
    float2 UV : TEXCOORD;
    float4 Color : COLOR;
    nointerpolation uint Primitive : PRIMITIVE;
};

float3 SrgbToLinear(float3 color)
{
    float3 low = color / 12.92;
    float3 high = pow(abs((color + 0.055) / 1.055), 2.4); // never negative; abs() satisfies fxc
    return color <= 0.04045 ? low : high;
}

PixelInput VertexMain(VertexInput input)
{
    PixelInput output;
    float2 normalized = input.Position / DisplaySize;
    // Direct3D's clip space has y pointing up, like WebGPU's.
    output.Position = float4(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0, 0.0, 1.0);
    output.Local = input.Local;
    output.UV = input.UV;
    output.Primitive = input.Primitive;
    // Colors are authored in sRGB. Unorm targets take them as they are (blending in gamma space, as macOS UI
    // does); sRGB targets need linear values.
    output.Color = float4(lerp(input.Color.rgb, SrgbToLinear(input.Color.rgb), LinearOutput), input.Color.a);
    return output;
}

float SquircleExtent(float2 halfSize, float radius, float smoothing)
{
    float limit = max(0.0, min(halfSize.x, halfSize.y));
    float clampedRadius = clamp(radius, 0.0, limit);
    return min(clampedRadius * (1.0 + clamp(smoothing, 0.0, 1.0)), limit);
}

float SquircleExponent(float radius, float extent)
{
    if (extent <= radius || radius <= 0.0)
        return 2.0;
    float apex = 0.29289322 * radius / extent;
    return -0.69314718 / log(1.0 - apex);
}

float SquircleDistance(float2 position, float2 halfSize, float radius, float smoothing)
{
    float extent = SquircleExtent(halfSize, radius, smoothing);
    float clampedRadius = clamp(radius, 0.0, max(0.0, min(halfSize.x, halfSize.y)));

    // Position relative to the inner corner of the corner patch, folded into the first quadrant.
    float2 q = abs(position) - (halfSize - extent);

    // Next to a straight edge: the distance to that edge.
    if (q.x <= 0.0 || q.y <= 0.0)
        return max(q.x, q.y) - extent;

    float exponent = SquircleExponent(clampedRadius, extent);
    if (exponent <= 2.0)
        return length(q) - extent;

    // Superellipse: f = (qx^n + qy^n)^(1/n) - p. Dividing f by the length of its gradient turns it into a
    // distance near the outline. The gradient's length depends on the direction, which is undefined at the
    // patch's inner corner, so the correction fades out away from the outline; f alone is continuous.
    float2 p = pow(abs(q), exponent); // q > 0 here; abs() satisfies fxc
    float norm = pow(p.x + p.y, 1.0 / exponent);
    float2 g = p / q;
    float gradient = length(g) / pow(norm, exponent - 1.0);
    float value = norm - extent;
    float fade = min(abs(value) / extent, 1.0);
    return value / lerp(gradient, 1.0, fade);
}

// Coverage of a pixel whose center is `distance` points from the outline.
float Coverage(float distance)
{
    return saturate(0.5 - distance * ContentScale);
}

float4 PixelMain(PixelInput input) : SV_Target
{
    uint4 first = Primitives.Load(int(input.Primitive) * 2);
    uint4 second = Primitives.Load(int(input.Primitive) * 2 + 1);
    float2 halfSize = asfloat(first.xy);
    float radius = asfloat(first.z);
    float smoothing = asfloat(first.w);
    float strokeWidth = asfloat(second.x);
    float softness = asfloat(second.y);
    uint kind = second.z;

    // Glyph UVs are in texels of the atlas, so they survive the atlas growing mid-frame. Image UVs are 0..1.
    float width;
    float height;
    ColorTexture.GetDimensions(width, height);
    float2 uv = kind == KindGlyph ? input.UV / float2(width, height) : input.UV;
    float4 texel = ColorTexture.SampleLevel(ColorSampler, uv, 0.0);

    float4 color = input.Color;
    float coverage = 1.0;
    if (kind == KindSquircle)
    {
        coverage = Coverage(SquircleDistance(input.Local, halfSize, radius, smoothing));
    }
    else if (kind == KindSquircleStroke)
    {
        float outer = SquircleDistance(input.Local, halfSize, radius, smoothing);
        float inner = SquircleDistance(input.Local, halfSize - strokeWidth, max(radius - strokeWidth, 0.0), smoothing);
        coverage = min(Coverage(outer), 1.0 - Coverage(inner));
    }
    else if (kind == KindShadow)
    {
        float distance = SquircleDistance(input.Local, halfSize, radius, smoothing);
        float falloff = saturate(0.5 - distance / max(2.0 * softness, 0.0001));
        coverage = falloff * falloff * (3.0 - 2.0 * falloff);
    }
    else if (kind == KindGlyph)
    {
        coverage = texel.r;
    }
    else if (kind == KindImage)
    {
        color = color * texel;
        coverage = Coverage(SquircleDistance(input.Local, halfSize, radius, smoothing));
    }

    // Premultiplied alpha output.
    float alpha = color.a * coverage;
    return float4(color.rgb * alpha, alpha);
}
