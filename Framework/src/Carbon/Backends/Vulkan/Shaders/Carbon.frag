#version 450

// Fragment stage of the Vulkan backend. A port of FragmentMain in Backends/WebGPU/Shaders/Carbon.wgsl; the squircle
// functions mirror Framework/src/Carbon/Draw/Squircle.cpp. Change them together.

layout(push_constant) uniform Frame
{
    vec2 DisplaySize;
    float ContentScale;
    float LinearOutput;
} frame;

struct Primitive
{
    vec2 HalfSize;
    float Radius;
    float Smoothing;
    float StrokeWidth;
    float Softness;
    uint Kind;
    uint Reserved;
};

const uint KindSquircle = 0u;
const uint KindSquircleStroke = 1u;
const uint KindShadow = 2u;
const uint KindGlyph = 3u;
const uint KindImage = 4u;

layout(std430, set = 0, binding = 0) readonly buffer Primitives
{
    Primitive primitives[];
};
layout(set = 1, binding = 0) uniform sampler2D colorTexture;

layout(location = 0) in vec2 inLocal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) flat in uint inPrimitive;

layout(location = 0) out vec4 outColor;

float SquircleExtent(vec2 halfSize, float radius, float smoothing)
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

float SquircleDistance(vec2 point, vec2 halfSize, float radius, float smoothing)
{
    float extent = SquircleExtent(halfSize, radius, smoothing);
    float clampedRadius = clamp(radius, 0.0, max(0.0, min(halfSize.x, halfSize.y)));

    // Position relative to the inner corner of the corner patch, folded into the first quadrant.
    vec2 q = abs(point) - (halfSize - vec2(extent));

    // Next to a straight edge: the distance to that edge.
    if (q.x <= 0.0 || q.y <= 0.0)
        return max(q.x, q.y) - extent;

    float exponent = SquircleExponent(clampedRadius, extent);
    if (exponent <= 2.0)
        return length(q) - extent;

    // Superellipse: f = (qx^n + qy^n)^(1/n) - p. Dividing f by the length of its gradient turns it into a
    // distance near the outline. The gradient's length depends on the direction, which is undefined at the
    // patch's inner corner, so the correction fades out away from the outline; f alone is continuous.
    vec2 p = pow(q, vec2(exponent));
    float norm = pow(p.x + p.y, 1.0 / exponent);
    vec2 g = p / q;
    float gradient = length(g) / pow(norm, exponent - 1.0);
    float value = norm - extent;
    float fade = min(abs(value) / extent, 1.0);
    return value / mix(gradient, 1.0, fade);
}

// Coverage of a pixel whose center is `distance` points from the outline.
float Coverage(float distance)
{
    return clamp(0.5 - distance * frame.ContentScale, 0.0, 1.0);
}

void main()
{
    Primitive primitive = primitives[inPrimitive];

    // Glyph UVs are in texels of the atlas, so they survive the atlas growing mid-frame. Image UVs are 0..1.
    vec2 dimensions = vec2(textureSize(colorTexture, 0));
    vec2 uv = primitive.Kind == KindGlyph ? inUV / dimensions : inUV;
    vec4 texel = textureLod(colorTexture, uv, 0.0);

    vec4 color = inColor;
    float coverage = 1.0;
    if (primitive.Kind == KindSquircle)
    {
        coverage = Coverage(SquircleDistance(inLocal, primitive.HalfSize, primitive.Radius, primitive.Smoothing));
    }
    else if (primitive.Kind == KindSquircleStroke)
    {
        float outer = SquircleDistance(inLocal, primitive.HalfSize, primitive.Radius, primitive.Smoothing);
        float inner = SquircleDistance(inLocal, primitive.HalfSize - vec2(primitive.StrokeWidth),
                                       max(primitive.Radius - primitive.StrokeWidth, 0.0), primitive.Smoothing);
        coverage = min(Coverage(outer), 1.0 - Coverage(inner));
    }
    else if (primitive.Kind == KindShadow)
    {
        float distance = SquircleDistance(inLocal, primitive.HalfSize, primitive.Radius, primitive.Smoothing);
        float falloff = clamp(0.5 - distance / max(2.0 * primitive.Softness, 0.0001), 0.0, 1.0);
        coverage = falloff * falloff * (3.0 - 2.0 * falloff);
    }
    else if (primitive.Kind == KindGlyph)
    {
        coverage = texel.r;
    }
    else if (primitive.Kind == KindImage)
    {
        color = color * texel;
        coverage = Coverage(SquircleDistance(inLocal, primitive.HalfSize, primitive.Radius, primitive.Smoothing));
    }

    // Premultiplied alpha output.
    float alpha = color.a * coverage;
    outColor = vec4(color.rgb * alpha, alpha);
}
