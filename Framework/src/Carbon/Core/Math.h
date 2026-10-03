#pragma once

#include <algorithm>
#include <cmath>

namespace Carbon
{
    /// Linear interpolation between a and b; t is not clamped.
    constexpr float Lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    /// Clamps a value to [0, 1].
    constexpr float Saturate(float value)
    {
        return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
    }

    /// Returns true when two values differ by at most epsilon.
    inline bool NearlyEqual(float a, float b, float epsilon = 1e-5f)
    {
        return std::abs(a - b) <= epsilon;
    }
} // namespace Carbon
