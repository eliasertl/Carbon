// The Direct3D 9 backend's shaders, shader model 3.0. A port of Backends/WebGPU/Shaders/Carbon.wgsl; the squircle
// functions mirror Framework/src/Carbon/Draw/Squircle.cpp. Change them together. fxc compiles VertexMain and
// PixelMain at build time.
//
// Shader model 3.0 has no integer attributes and no buffers a pixel shader can index, so the backend copies each
// vertex's primitive into the vertex itself (Shape and Stroke below). The four vertices of a quad carry the same
// values, so the interpolated values are exact.

// x, y: size of the display area in points. z, w: the offset of half a pixel in clip space, which moves Direct3D 9's
// pixel centers onto those of the other APIs.
float4 Frame : register(c0);
// x: pixels per point. y: 1.0 when the render target expects linear color values (D3DRS_SRGBWRITEENABLE). z, w: one
// over the size of the glyph atlas bound, for the commands that sample one of the atlases.
float4 Output : register(c1);

sampler2D ColorTexture : register(s0);

static const float KindSquircle = 0.0;
static const float KindSquircleStroke = 1.0;
static const float KindShadow = 2.0;
static const float KindGlyph = 3.0;
static const float KindImage = 4.0;
static const float KindColorGlyph = 5.0;

struct VertexInput
{
    float2 Position : POSITION;
    float2 Local : TEXCOORD0;
    float2 UV : TEXCOORD1;
    float4 Color : COLOR0;
    // Half size, radius, smoothing.
    float4 Shape : TEXCOORD2;
    // Stroke width, softness, kind.
    float3 Stroke : TEXCOORD3;
};

struct PixelInput
{
    float4 Position : POSITION;
    float2 Local : TEXCOORD0;
    float2 UV : TEXCOORD1;
    float4 Color : COLOR0;
    float4 Shape : TEXCOORD2;
    float3 Stroke : TEXCOORD3;
};

struct PixelShaderInput
{
    float2 Local : TEXCOORD0;
    float2 UV : TEXCOORD1;
    float4 Color : COLOR0;
    float4 Shape : TEXCOORD2;
    float3 Stroke : TEXCOORD3;
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
    float2 normalized = input.Position / Frame.xy;
    // Direct3D's clip space has y pointing up, like WebGPU's.
    output.Position = float4(normalized.x * 2.0 - 1.0 + Frame.z, 1.0 - normalized.y * 2.0 + Frame.w, 0.0, 1.0);
    output.Local = input.Local;
    output.UV = input.UV;
    output.Shape = input.Shape;
    output.Stroke = input.Stroke;
    // Colors are authored in sRGB. Unorm targets take them as they are (blending in gamma space, as macOS UI
    // does); sRGB targets need linear values.
    output.Color = float4(lerp(input.Color.rgb, SrgbToLinear(input.Color.rgb), Output.y), input.Color.a);
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
    return saturate(0.5 - distance * Output.x);
}

float4 PixelMain(PixelShaderInput input) : COLOR0
{
    float2 halfSize = input.Shape.xy;
    float radius = input.Shape.z;
    float smoothing = input.Shape.w;
    float strokeWidth = input.Stroke.x;
    float softness = input.Stroke.y;
    float kind = floor(input.Stroke.z + 0.5);

    // Glyph UVs are in texels of their atlas, so they survive the atlas growing mid-frame. Image UVs are 0..1. The
    // texture is read with an explicit level, which is allowed inside the branches below.
    bool isGlyph = kind == KindGlyph || kind == KindColorGlyph;
    float2 uv = isGlyph ? input.UV * Output.zw : input.UV;
    float4 texel = tex2Dlod(ColorTexture, float4(uv, 0.0, 0.0));

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
    else if (kind == KindColorGlyph)
    {
        // The color atlas is premultiplied and sRGB-encoded; sRGB targets need its colors linear. The vertex color
        // tints and fades it (white draws it as it is).
        float3 straight = texel.rgb / max(texel.a, 0.0001);
        float3 rgb = lerp(texel.rgb, SrgbToLinear(straight) * texel.a, Output.y);
        return float4(rgb * color.rgb, texel.a) * color.a;
    }

    // Premultiplied alpha output.
    float alpha = color.a * coverage;
    return float4(color.rgb * alpha, alpha);
}
