// Carbon's only shader. Every shape is a quad; the fragment stage evaluates a signed-distance function per pixel,
// so edges are antialiased analytically at any content scale.
//
// The squircle functions mirror Framework/src/Carbon/Draw/Squircle.cpp. Change both together.

struct Frame {
    // Size of the display area in points.
    displaySize: vec2f,
    // Pixels per point.
    contentScale: f32,
    // 1.0 when the render target has an sRGB format and expects linear color values.
    linearOutput: f32,
}

struct Primitive {
    halfSize: vec2f,
    radius: f32,
    smoothing: f32,
    strokeWidth: f32,
    softness: f32,
    kind: u32,
    reserved: u32,
}

const KindSquircle: u32 = 0u;
const KindSquircleStroke: u32 = 1u;
const KindShadow: u32 = 2u;
const KindGlyph: u32 = 3u;
const KindImage: u32 = 4u;
const KindColorGlyph: u32 = 5u;

@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<storage, read> primitives: array<Primitive>;
@group(1) @binding(0) var colorTexture: texture_2d<f32>;
@group(1) @binding(1) var colorSampler: sampler;

struct VertexInput {
    @location(0) position: vec2f,
    @location(1) local: vec2f,
    @location(2) uv: vec2f,
    @location(3) color: vec4f,
    @location(4) primitive: u32,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) local: vec2f,
    @location(1) uv: vec2f,
    @location(2) color: vec4f,
    @location(3) @interpolate(flat) primitive: u32,
}

fn SrgbToLinear(color: vec3f) -> vec3f {
    let low = color / 12.92;
    let high = pow((color + vec3f(0.055)) / 1.055, vec3f(2.4));
    return select(high, low, color <= vec3f(0.04045));
}

@vertex
fn VertexMain(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    let normalized = input.position / frame.displaySize;
    output.position = vec4f(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0, 0.0, 1.0);
    output.local = input.local;
    output.uv = input.uv;
    output.primitive = input.primitive;
    // Colors are authored in sRGB. Unorm targets take them as they are (blending in gamma space, as macOS UI
    // does); sRGB targets need linear values.
    output.color = vec4f(mix(input.color.rgb, SrgbToLinear(input.color.rgb), frame.linearOutput), input.color.a);
    return output;
}

fn SquircleExtent(halfSize: vec2f, radius: f32, smoothing: f32) -> f32 {
    let limit = max(0.0, min(halfSize.x, halfSize.y));
    let clampedRadius = clamp(radius, 0.0, limit);
    return min(clampedRadius * (1.0 + clamp(smoothing, 0.0, 1.0)), limit);
}

fn SquircleExponent(radius: f32, extent: f32) -> f32 {
    if (extent <= radius || radius <= 0.0) {
        return 2.0;
    }
    let apex = 0.29289322 * radius / extent;
    return -0.69314718 / log(1.0 - apex);
}

fn SquircleDistance(point: vec2f, halfSize: vec2f, radius: f32, smoothing: f32) -> f32 {
    let extent = SquircleExtent(halfSize, radius, smoothing);
    let clampedRadius = clamp(radius, 0.0, max(0.0, min(halfSize.x, halfSize.y)));

    // Position relative to the inner corner of the corner patch, folded into the first quadrant.
    let q = abs(point) - (halfSize - vec2f(extent));

    // Next to a straight edge: the distance to that edge.
    if (q.x <= 0.0 || q.y <= 0.0) {
        return max(q.x, q.y) - extent;
    }

    let exponent = SquircleExponent(clampedRadius, extent);
    if (exponent <= 2.0) {
        return length(q) - extent;
    }

    // Superellipse: f = (qx^n + qy^n)^(1/n) - p. Dividing f by the length of its gradient turns it into a
    // distance near the outline. The gradient's length depends on the direction, which is undefined at the
    // patch's inner corner, so the correction fades out away from the outline; f alone is continuous.
    let p = pow(q, vec2f(exponent));
    let norm = pow(p.x + p.y, 1.0 / exponent);
    let g = p / q;
    let gradient = length(g) / pow(norm, exponent - 1.0);
    let value = norm - extent;
    let fade = min(abs(value) / extent, 1.0);
    return value / mix(gradient, 1.0, fade);
}

// Coverage of a pixel whose center is `distance` points from the outline.
fn Coverage(distance: f32) -> f32 {
    return clamp(0.5 - distance * frame.contentScale, 0.0, 1.0);
}

@fragment
fn FragmentMain(input: VertexOutput) -> @location(0) vec4f {
    let primitive = primitives[input.primitive];

    // Glyph UVs are in texels of their atlas, so they survive the atlas growing mid-frame. Image UVs are 0..1.
    let dimensions = vec2f(textureDimensions(colorTexture));
    let isGlyph = primitive.kind == KindGlyph || primitive.kind == KindColorGlyph;
    let uv = select(input.uv, input.uv / dimensions, isGlyph);
    let texel = textureSampleLevel(colorTexture, colorSampler, uv, 0.0);

    var color = input.color;
    var coverage = 1.0;
    switch primitive.kind {
        case KindSquircle: {
            coverage = Coverage(SquircleDistance(input.local, primitive.halfSize, primitive.radius, primitive.smoothing));
        }
        case KindSquircleStroke: {
            let outer = SquircleDistance(input.local, primitive.halfSize, primitive.radius, primitive.smoothing);
            let inner = SquircleDistance(input.local, primitive.halfSize - vec2f(primitive.strokeWidth),
                                         max(primitive.radius - primitive.strokeWidth, 0.0), primitive.smoothing);
            coverage = min(Coverage(outer), 1.0 - Coverage(inner));
        }
        case KindShadow: {
            let distance = SquircleDistance(input.local, primitive.halfSize, primitive.radius, primitive.smoothing);
            let falloff = clamp(0.5 - distance / max(2.0 * primitive.softness, 0.0001), 0.0, 1.0);
            coverage = falloff * falloff * (3.0 - 2.0 * falloff);
        }
        case KindGlyph: {
            coverage = texel.r;
        }
        case KindImage: {
            color = color * texel;
            coverage = Coverage(SquircleDistance(input.local, primitive.halfSize, primitive.radius, primitive.smoothing));
        }
        case KindColorGlyph: {
            // The color atlas is premultiplied and sRGB-encoded; sRGB targets need its colors linear. The vertex
            // color tints and fades it (white draws it as it is).
            let straight = texel.rgb / max(texel.a, 0.0001);
            let rgb = mix(texel.rgb, SrgbToLinear(straight) * texel.a, frame.linearOutput);
            return vec4f(rgb * color.rgb, texel.a) * color.a;
        }
        default: {
        }
    }

    // Premultiplied alpha output.
    let alpha = color.a * coverage;
    return vec4f(color.rgb * alpha, alpha);
}
