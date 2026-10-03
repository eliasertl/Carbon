#pragma once

namespace Carbon
{
    /// Distances from the four edges of a rectangle, used for padding and margins.
    struct EdgeInsets
    {
        float Left = 0.0f;
        float Top = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;

        constexpr EdgeInsets() = default;
        /// The same inset on all four edges. Implicit so `.Padding = 20.0f` works.
        constexpr EdgeInsets(float all) : Left(all), Top(all), Right(all), Bottom(all) {}
        /// Horizontal inset for left and right, vertical inset for top and bottom.
        constexpr EdgeInsets(float horizontal, float vertical)
            : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical)
        {
        }
        constexpr EdgeInsets(float left, float top, float right, float bottom)
            : Left(left), Top(top), Right(right), Bottom(bottom)
        {
        }

        /// Left plus right.
        constexpr float GetHorizontal() const { return Left + Right; }
        /// Top plus bottom.
        constexpr float GetVertical() const { return Top + Bottom; }

        constexpr bool operator==(const EdgeInsets& other) const = default;
    };
} // namespace Carbon
