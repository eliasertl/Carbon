#include "Carbon/Draw/Squircle.h"

#include <algorithm>
#include <cmath>

namespace Carbon
{
    // NOTE: Renderer/Shaders/Carbon.wgsl contains the same math. Change both together.

    float GetSquircleExtent(Vec2 halfSize, float radius, float smoothing)
    {
        const float limit = std::max(0.0f, std::min(halfSize.X, halfSize.Y));
        const float clampedRadius = std::clamp(radius, 0.0f, limit);
        return std::min(clampedRadius * (1.0f + std::clamp(smoothing, 0.0f, 1.0f)), limit);
    }

    float GetSquircleExponent(float radius, float extent)
    {
        if (extent <= radius || radius <= 0.0f)
            return 2.0f;
        // Apex of a circle of radius r:      r * (1 - 1/sqrt(2)) from the corner, along each axis.
        // Apex of the superellipse patch:    p * (1 - 2^(-1/n)).
        // Setting them equal and solving for n:
        const float apex = 0.29289322f * radius / extent;
        return -0.69314718f / std::log(1.0f - apex);
    }

    float SquircleDistance(Vec2 point, Vec2 halfSize, float radius, float smoothing)
    {
        const float extent = GetSquircleExtent(halfSize, radius, smoothing);
        const float clampedRadius = std::clamp(radius, 0.0f, std::max(0.0f, std::min(halfSize.X, halfSize.Y)));

        // Position relative to the inner corner of the patch, folded into the first quadrant.
        const float qx = std::abs(point.X) - (halfSize.X - extent);
        const float qy = std::abs(point.Y) - (halfSize.Y - extent);

        // Next to a straight edge: the distance to that edge.
        if (qx <= 0.0f || qy <= 0.0f)
            return std::max(qx, qy) - extent;

        const float exponent = GetSquircleExponent(clampedRadius, extent);
        if (exponent <= 2.0f)
            return std::sqrt(qx * qx + qy * qy) - extent;

        // Superellipse: f = (qx^n + qy^n)^(1/n) - p. Dividing f by the length of its gradient turns it into a
        // distance near the outline. The gradient's length depends on the direction, which is undefined at the
        // patch's inner corner, so the correction fades out away from the outline; f alone is continuous.
        const float px = std::pow(qx, exponent);
        const float py = std::pow(qy, exponent);
        const float norm = std::pow(px + py, 1.0f / exponent);
        const float gx = px / qx; // qx^(n-1)
        const float gy = py / qy; // qy^(n-1)
        const float gradient = std::sqrt(gx * gx + gy * gy) / std::pow(norm, exponent - 1.0f);
        const float value = norm - extent;
        const float fade = std::min(std::abs(value) / extent, 1.0f);
        return value / (gradient + (1.0f - gradient) * fade);
    }

    float SquircleCoverage(float distance, float contentScale)
    {
        return std::clamp(0.5f - distance * contentScale, 0.0f, 1.0f);
    }
} // namespace Carbon
