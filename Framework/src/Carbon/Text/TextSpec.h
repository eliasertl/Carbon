#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

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

    /// Where the lines of a text sit inside its maximum width.
    enum class TextAlignment : uint8_t
    {
        Leading,
        Center,
        Trailing
    };

    /// Everything that determines how a piece of text is shaped, laid out and rasterized.
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
        /// The width available to the text, in points; 0 means unlimited. Text that is wider wraps or is cut off.
        float MaxWidth = 0.0f;
        /// With a MaxWidth: true breaks lines between words; false keeps one line and ends it with an ellipsis.
        bool Wraps = true;
        /// Where lines sit inside MaxWidth.
        TextAlignment Alignment = TextAlignment::Leading;
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

    /// Measures text in points. Lines are separated by '\n' and wrap at the spec's MaxWidth; the result is the
    /// widest line by the total height.
    Vec2 MeasureText(std::string_view text, const TextSpec& spec);

    /// Fills `positions` with the horizontal position of the text caret, in points from the start of the line,
    /// before each byte of a single line of text and after its last one (text.size() + 1 entries). Text editors
    /// use it to place the caret and to turn a click into a text offset.
    void GetCaretPositions(std::string_view line, const TextSpec& spec, std::vector<float>& positions);
} // namespace Carbon
