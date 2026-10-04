#pragma once

#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// The shape function behind every rounded shape in Carbon: a rectangle whose corners have continuous
    /// curvature ("squircle"), like Apple's. The fragment shader mirrors these functions line by line; the CPU
    /// versions exist for tests and for hit testing.
    ///
    /// Model: each corner is a superellipse patch |u/p|^n + |v/p|^n = 1.
    ///  - `radius` clamps to half the shortest side.
    ///  - The patch reaches `p = radius * (1 + smoothing)` along each edge, clamped to half the shortest side. When
    ///    the clamp bites there is less room to smooth, so pills and circles end up as exact circular arcs.
    ///  - The exponent `n` is chosen so the curve passes through the same 45-degree apex as a circular corner of
    ///    `radius`. Smoothing 0 gives n = 2: an exact circular rounded rectangle. For n > 2 the curvature is zero
    ///    where the patch meets the straight edge, which is what makes the corner look smooth.

    /// How far the corner patch reaches along each edge, measured from the corner of the rectangle.
    float GetSquircleExtent(Vec2 halfSize, float radius, float smoothing);

    /// The superellipse exponent for a corner of `radius` whose patch reaches `extent`. 2 is a circle.
    float GetSquircleExponent(float radius, float extent);

    /// Signed distance, in the units of the inputs, from `point` (relative to the shape's center) to the outline:
    /// negative inside, positive outside. Exact along the straight edges and for circular corners, and a
    /// first-order estimate near smoothed corners, which is what antialiasing and thin strokes need.
    float SquircleDistance(Vec2 point, Vec2 halfSize, float radius, float smoothing);

    /// Antialiased coverage (0..1) of a pixel whose center is `distance` points from the outline.
    float SquircleCoverage(float distance, float contentScale);
} // namespace Carbon
