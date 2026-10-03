#pragma once

#include <cmath>

#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// Converts between logical points (what the API uses) and physical pixels (what the renderer and the glyph
    /// rasterizer use). A factor of 1.5 corresponds to a display scaled to 150 %.
    struct ContentScale
    {
        float Factor = 1.0f;

        constexpr float ToPixels(float points) const { return points * Factor; }
        constexpr Vec2 ToPixels(Vec2 points) const { return points * Factor; }
        constexpr float ToPoints(float pixels) const { return pixels / Factor; }
        constexpr Vec2 ToPoints(Vec2 pixels) const { return pixels / Factor; }

        /// The size of one physical pixel, in points.
        constexpr float GetPixelSize() const { return 1.0f / Factor; }

        /// Moves a coordinate to the nearest pixel boundary; the result is still in points.
        float Snap(float points) const { return std::round(points * Factor) / Factor; }
        Vec2 Snap(Vec2 points) const { return Vec2(Snap(points.X), Snap(points.Y)); }

        /// Snaps both corners of a rectangle to pixel boundaries, so its edges render without blur.
        Rect Snap(const Rect& rect) const { return Rect::FromMinMax(Snap(rect.GetMin()), Snap(rect.GetMax())); }
    };
} // namespace Carbon
