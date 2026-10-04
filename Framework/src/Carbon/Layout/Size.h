#pragma once

#include <cstdint>

namespace Carbon
{
    /// How an item or container is sized along one axis.
    enum class SizeMode : uint8_t
    {
        /// As large as its content.
        Fit,
        /// A fixed number of points.
        Fixed,
        /// A share of the space the parent has left, proportional to a weight.
        Fill
    };

    /// A size along one axis: fit the content, a fixed length, or fill the available space.
    struct Size
    {
        SizeMode Mode = SizeMode::Fit;
        /// Fixed: the length in points. Fill: the weight.
        float Value = 0.0f;

        constexpr Size() = default;
        /// A fixed length in points. Implicit, so `.Width = 200.0f` works.
        constexpr Size(float points) : Mode(SizeMode::Fixed), Value(points) {}
        constexpr Size(SizeMode mode, float value) : Mode(mode), Value(value) {}

        /// As large as the content.
        static constexpr Size Fit() { return Size(SizeMode::Fit, 0.0f); }
        /// Exactly `points`.
        static constexpr Size Fixed(float points) { return Size(SizeMode::Fixed, points); }
        /// A share of the parent's free space. Items share it in proportion to their weights.
        static constexpr Size Fill(float weight = 1.0f) { return Size(SizeMode::Fill, weight); }

        constexpr bool operator==(const Size& other) const = default;
    };

    /// Horizontal placement: of items across a vertical stack, or of content along a horizontal one.
    enum class Alignment : uint8_t
    {
        Leading,
        Center,
        Trailing
    };

    /// Vertical placement: of items across a horizontal stack, or of content along a vertical one.
    enum class VerticalAlignment : uint8_t
    {
        Top,
        Center,
        Bottom
    };

    /// The direction a container lays its items out in.
    enum class Axis : uint8_t
    {
        Horizontal,
        Vertical
    };
} // namespace Carbon
