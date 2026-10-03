#pragma once

#include <cstdint>

namespace Carbon
{
    /// An sRGB color with straight (non-premultiplied) alpha; components are in [0, 1].
    struct Color
    {
        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;
        float A = 1.0f;

        constexpr Color() = default;
        constexpr Color(float r, float g, float b, float a = 1.0f) : R(r), G(g), B(b), A(a) {}

        /// Builds a color from 8-bit components.
        static constexpr Color FromRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
        {
            return Color(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, float(a) / 255.0f);
        }

        /// Builds a color from 0xRRGGBB, e.g. Color::FromHex(0x007AFF).
        static constexpr Color FromHex(uint32_t rgb, float alpha = 1.0f)
        {
            return Color(float((rgb >> 16) & 0xFF) / 255.0f, float((rgb >> 8) & 0xFF) / 255.0f,
                         float(rgb & 0xFF) / 255.0f, alpha);
        }

        static constexpr Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static constexpr Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static constexpr Color Transparent() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }

        /// The same color with a different alpha.
        constexpr Color WithAlpha(float alpha) const { return Color(R, G, B, alpha); }
        /// The same color with its alpha multiplied by `opacity`.
        constexpr Color WithOpacity(float opacity) const { return Color(R, G, B, A * opacity); }

        /// Packs to 8 bits per channel with R in the lowest byte (memory order R, G, B, A on little-endian).
        constexpr uint32_t ToRGBA8() const
        {
            return uint32_t(ToByte(R)) | (uint32_t(ToByte(G)) << 8) | (uint32_t(ToByte(B)) << 16) |
                   (uint32_t(ToByte(A)) << 24);
        }

        constexpr bool operator==(const Color& other) const = default;

    private:
        static constexpr uint8_t ToByte(float value)
        {
            const float clamped = value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
            return uint8_t(clamped * 255.0f + 0.5f);
        }
    };

    /// Component-wise interpolation of two colors, weighted by alpha so transparent ends do not tint the blend.
    constexpr Color Lerp(const Color& a, const Color& b, float t)
    {
        const float alpha = a.A + (b.A - a.A) * t;
        if (alpha <= 0.0f)
            return Color(b.R, b.G, b.B, 0.0f);
        const float weightA = a.A * (1.0f - t);
        const float weightB = b.A * t;
        return Color((a.R * weightA + b.R * weightB) / alpha, (a.G * weightA + b.G * weightB) / alpha,
                     (a.B * weightA + b.B * weightB) / alpha, alpha);
    }
} // namespace Carbon
