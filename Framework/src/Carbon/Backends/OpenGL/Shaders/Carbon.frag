// Fragment stage of the OpenGL and OpenGL ES backends, GLSL 3.30 core and GLSL ES 3.00. The backend puts the
// #version line (and for ES the default precisions) in front. A port of FragmentMain in
// Backends/WebGPU/Shaders/Carbon.wgsl; the squircle functions mirror Framework/src/Carbon/Draw/Squircle.cpp. Change
// them together.

uniform vec4 Frame;
// The texture of the draw command: the glyph atlas or a host texture.
uniform sampler2D ColorTexture;
// The frame's primitives, two RGBA32UI texels each: (half size, radius, smoothing) and (stroke width, softness,
// kind, reserved), 1024 primitives per row. Floats are stored as their bits, so an integer format returns them
// exactly.
uniform usampler2D Primitives;
const int PrimitivesPerRow = 1024;

const uint KindSquircle = 0u;
const uint KindSquircleStroke = 1u;
const uint KindShadow = 2u;
const uint KindGlyph = 3u;
const uint KindImage = 4u;

in vec2 vLocal;
in vec2 vUV;
in vec4 vColor;
flat in uint vPrimitive;

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
    return clamp(0.5 - distance * Frame.z, 0.0, 1.0);
}

void main()
{
    ivec2 primitiveTexel = ivec2((int(vPrimitive) % PrimitivesPerRow) * 2, int(vPrimitive) / PrimitivesPerRow);
    uvec4 first = texelFetch(Primitives, primitiveTexel, 0);
    uvec4 second = texelFetch(Primitives, primitiveTexel + ivec2(1, 0), 0);
    vec2 halfSize = uintBitsToFloat(first.xy);
    float radius = uintBitsToFloat(first.z);
    float smoothing = uintBitsToFloat(first.w);
    float strokeWidth = uintBitsToFloat(second.x);
    float softness = uintBitsToFloat(second.y);
    uint kind = second.z;

    // Glyph UVs are in texels of the atlas, so they survive the atlas growing mid-frame. Image UVs are 0..1.
    vec2 dimensions = vec2(textureSize(ColorTexture, 0));
    vec2 uv = kind == KindGlyph ? vUV / dimensions : vUV;
    vec4 texel = textureLod(ColorTexture, uv, 0.0);

    vec4 color = vColor;
    float coverage = 1.0;
    if (kind == KindSquircle)
    {
        coverage = Coverage(SquircleDistance(vLocal, halfSize, radius, smoothing));
    }
    else if (kind == KindSquircleStroke)
    {
        float outer = SquircleDistance(vLocal, halfSize, radius, smoothing);
        float inner =
            SquircleDistance(vLocal, halfSize - vec2(strokeWidth), max(radius - strokeWidth, 0.0), smoothing);
        coverage = min(Coverage(outer), 1.0 - Coverage(inner));
    }
    else if (kind == KindShadow)
    {
        float distance = SquircleDistance(vLocal, halfSize, radius, smoothing);
        float falloff = clamp(0.5 - distance / max(2.0 * softness, 0.0001), 0.0, 1.0);
        coverage = falloff * falloff * (3.0 - 2.0 * falloff);
    }
    else if (kind == KindGlyph)
    {
        coverage = texel.r;
    }
    else if (kind == KindImage)
    {
        color = color * texel;
        coverage = Coverage(SquircleDistance(vLocal, halfSize, radius, smoothing));
    }

    // Premultiplied alpha output.
    float alpha = color.a * coverage;
    outColor = vec4(color.rgb * alpha, alpha);
}
