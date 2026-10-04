#pragma once

#include <cstdint>

namespace Carbon
{
    /// Timing curves for duration-based animations.
    enum class Easing : uint8_t
    {
        Linear,
        /// Starts slowly, ends at full speed.
        EaseIn,
        /// Starts at full speed, ends slowly.
        EaseOut,
        /// Starts and ends slowly.
        EaseInOut
    };

    /// Maps linear progress `t` in [0, 1] to eased progress. Values outside the range are clamped; every curve
    /// maps 0 to 0 and 1 to 1.
    float Ease(Easing easing, float t);

    /// Evaluates the cubic Bezier timing curve through (0, 0), (x1, y1), (x2, y2), (1, 1) at horizontal position
    /// `t`, like CSS `cubic-bezier()`.
    float CubicBezier(float x1, float y1, float x2, float y2, float t);
} // namespace Carbon
