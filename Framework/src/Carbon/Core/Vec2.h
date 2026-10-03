#pragma once

#include <cmath>

namespace Carbon
{
    /// A 2D vector or point, in logical points unless stated otherwise.
    struct Vec2
    {
        float X = 0.0f;
        float Y = 0.0f;

        constexpr Vec2() = default;
        constexpr Vec2(float x, float y) : X(x), Y(y) {}
        /// Both components set to the same value.
        constexpr explicit Vec2(float value) : X(value), Y(value) {}

        constexpr Vec2 operator+(Vec2 other) const { return Vec2(X + other.X, Y + other.Y); }
        constexpr Vec2 operator-(Vec2 other) const { return Vec2(X - other.X, Y - other.Y); }
        constexpr Vec2 operator*(Vec2 other) const { return Vec2(X * other.X, Y * other.Y); }
        constexpr Vec2 operator/(Vec2 other) const { return Vec2(X / other.X, Y / other.Y); }
        constexpr Vec2 operator*(float scalar) const { return Vec2(X * scalar, Y * scalar); }
        constexpr Vec2 operator/(float scalar) const { return Vec2(X / scalar, Y / scalar); }
        constexpr Vec2 operator-() const { return Vec2(-X, -Y); }

        constexpr Vec2& operator+=(Vec2 other)
        {
            X += other.X;
            Y += other.Y;
            return *this;
        }

        constexpr Vec2& operator-=(Vec2 other)
        {
            X -= other.X;
            Y -= other.Y;
            return *this;
        }

        constexpr Vec2& operator*=(float scalar)
        {
            X *= scalar;
            Y *= scalar;
            return *this;
        }

        constexpr bool operator==(const Vec2& other) const = default;

        /// Euclidean length.
        float GetLength() const { return std::sqrt(X * X + Y * Y); }
        /// Squared length; cheaper than GetLength for comparisons.
        constexpr float GetLengthSquared() const { return X * X + Y * Y; }
    };

    constexpr Vec2 operator*(float scalar, Vec2 vector)
    {
        return vector * scalar;
    }

    /// Dot product.
    constexpr float Dot(Vec2 a, Vec2 b)
    {
        return a.X * b.X + a.Y * b.Y;
    }

    /// Component-wise linear interpolation.
    constexpr Vec2 Lerp(Vec2 a, Vec2 b, float t)
    {
        return Vec2(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t);
    }

    /// Component-wise minimum.
    constexpr Vec2 Min(Vec2 a, Vec2 b)
    {
        return Vec2(a.X < b.X ? a.X : b.X, a.Y < b.Y ? a.Y : b.Y);
    }

    /// Component-wise maximum.
    constexpr Vec2 Max(Vec2 a, Vec2 b)
    {
        return Vec2(a.X > b.X ? a.X : b.X, a.Y > b.Y ? a.Y : b.Y);
    }
} // namespace Carbon
