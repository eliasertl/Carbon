#pragma once

#include <cstdint>
#include <string_view>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Text/Font.h"

namespace Carbon
{
    /// Which of the embedded Phosphor icon fonts draws icon characters.
    enum class IconVariant : uint8_t
    {
        /// Follow the text weight: semibold and heavier text uses the bold icons.
        Auto,
        Regular,
        Bold,
        Fill
    };

    /// Everything that determines how a piece of text is shaped and rasterized.
    struct TextSpec
    {
        /// The font family; null selects the default font.
        Carbon::Font* Font = nullptr;
        /// Font size in points.
        float Size = 13.0f;
        FontWeight Weight = FontWeight::Regular;
        bool Italic = false;
        /// Height of a line in points; 0 uses the font's natural line height.
        float LineHeight = 0.0f;
        /// Extra space between characters, in points.
        float Tracking = 0.0f;
        IconVariant Icons = IconVariant::Auto;
    };

    /// Vertical metrics of a TextSpec, in points.
    struct FontMetrics
    {
        /// Distance from the baseline to the top of the tallest glyphs.
        float Ascent = 0.0f;
        /// Distance from the baseline to the bottom of the lowest glyphs (positive).
        float Descent = 0.0f;
        /// Height of capital letters.
        float CapHeight = 0.0f;
        /// Height of one line: TextSpec::LineHeight, or the font's natural line height.
        float LineHeight = 0.0f;
        /// Distance from the top of a line to its baseline; the glyphs are centered vertically in the line.
        float Baseline = 0.0f;
    };

    /// Returns the vertical metrics for a spec.
    FontMetrics GetFontMetrics(const TextSpec& spec);

    /// Measures text in points. Lines are separated by '\n'; the result is the widest line by the total height.
    Vec2 MeasureText(std::string_view text, const TextSpec& spec);
} // namespace Carbon
