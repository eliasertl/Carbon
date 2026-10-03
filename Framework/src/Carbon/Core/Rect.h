#pragma once

#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// An axis-aligned rectangle: origin (top-left) plus size, in logical points. Y grows downwards.
    struct Rect
    {
        float X = 0.0f;
        float Y = 0.0f;
        float Width = 0.0f;
        float Height = 0.0f;

        constexpr Rect() = default;
        constexpr Rect(float x, float y, float width, float height) : X(x), Y(y), Width(width), Height(height) {}
        constexpr Rect(Vec2 origin, Vec2 size) : X(origin.X), Y(origin.Y), Width(size.X), Height(size.Y) {}

        /// Builds a rectangle from its top-left and bottom-right corners.
        static constexpr Rect FromMinMax(Vec2 min, Vec2 max)
        {
            return Rect(min.X, min.Y, max.X - min.X, max.Y - min.Y);
        }
        /// Builds a rectangle from its center and size.
        static constexpr Rect FromCenter(Vec2 center, Vec2 size)
        {
            return Rect(center.X - size.X * 0.5f, center.Y - size.Y * 0.5f, size.X, size.Y);
        }

        constexpr Vec2 GetMin() const { return Vec2(X, Y); }
        constexpr Vec2 GetMax() const { return Vec2(X + Width, Y + Height); }
        constexpr Vec2 GetSize() const { return Vec2(Width, Height); }
        constexpr Vec2 GetCenter() const { return Vec2(X + Width * 0.5f, Y + Height * 0.5f); }
        constexpr float GetRight() const { return X + Width; }
        constexpr float GetBottom() const { return Y + Height; }

        /// True when the rectangle has no area.
        constexpr bool IsEmpty() const { return Width <= 0.0f || Height <= 0.0f; }

        /// True when the point lies inside; the right and bottom edges are exclusive.
        constexpr bool Contains(Vec2 point) const
        {
            return point.X >= X && point.Y >= Y && point.X < X + Width && point.Y < Y + Height;
        }

        /// True when the two rectangles share any area.
        constexpr bool Intersects(const Rect& other) const
        {
            return X < other.X + other.Width && other.X < X + Width && Y < other.Y + other.Height &&
                   other.Y < Y + Height;
        }

        /// The shared area of two rectangles; empty (zero size) when they do not overlap.
        constexpr Rect GetIntersection(const Rect& other) const
        {
            const Vec2 min = Max(GetMin(), other.GetMin());
            const Vec2 max = Max(min, Min(GetMax(), other.GetMax()));
            return FromMinMax(min, max);
        }

        /// The smallest rectangle containing both.
        constexpr Rect GetUnion(const Rect& other) const
        {
            return FromMinMax(Min(GetMin(), other.GetMin()), Max(GetMax(), other.GetMax()));
        }

        /// Shrinks the rectangle by the insets; the size never becomes negative.
        constexpr Rect Inset(const EdgeInsets& insets) const
        {
            const float width = Width - insets.GetHorizontal();
            const float height = Height - insets.GetVertical();
            return Rect(X + insets.Left, Y + insets.Top, width > 0.0f ? width : 0.0f, height > 0.0f ? height : 0.0f);
        }

        /// Grows the rectangle by `amount` on every side (negative shrinks).
        constexpr Rect Expand(float amount) const
        {
            return Rect(X - amount, Y - amount, Width + amount * 2.0f, Height + amount * 2.0f);
        }

        /// Moves the rectangle.
        constexpr Rect Offset(Vec2 delta) const { return Rect(X + delta.X, Y + delta.Y, Width, Height); }

        constexpr bool operator==(const Rect& other) const = default;
    };

    /// Interpolates origin and size.
    constexpr Rect Lerp(const Rect& a, const Rect& b, float t)
    {
        return Rect(Lerp(a.GetMin(), b.GetMin(), t), Lerp(a.GetSize(), b.GetSize(), t));
    }
} // namespace Carbon
