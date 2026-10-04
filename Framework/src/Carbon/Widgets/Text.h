#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Core/Color.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Text/Font.h"
#include "Carbon/Text/TextSpec.h"
#include "Carbon/Text/TextStyle.h"

namespace Carbon
{
    /// Options of Text. All fields are optional.
    struct TextOptions
    {
        /// The text style from the macOS type ramp.
        TextStyle Style = TextStyle::Body;
        /// Uses the style's emphasized weight (bold titles, semibold body text).
        bool Emphasized = false;
        /// Uses the secondary label color, for subtitles and explanations.
        bool Secondary = false;
        /// The text color; the theme's Label (or SecondaryLabel) when not set.
        std::optional<Carbon::Color> Color = {};
        /// Overrides the style's weight.
        std::optional<FontWeight> Weight = {};
        bool Italic = false;
        /// Fit: as wide as the text. Fixed or Fill: the text is laid out inside that width.
        Size Width = Size::Fit();
        /// With a Fixed or Fill width: true wraps between words; false keeps one line, ending in an ellipsis.
        bool Wraps = false;
        /// Where lines sit inside a Fixed or Fill width.
        TextAlignment Alignment = TextAlignment::Leading;
        /// Which Phosphor font draws icons inside the text.
        IconVariant Icons = IconVariant::Auto;
    };

    /// Displays text. Lines are separated by '\n'. Icons from Carbon::Icons can be part of the text.
    void Text(std::string_view text, const TextOptions& options = {});

    /// Options of Icon. All fields are optional.
    struct IconOptions
    {
        /// Edge length of the icon's square, in points.
        float Size = 16.0f;
        /// The icon color; the theme's Label when not set.
        std::optional<Carbon::Color> Color = {};
        /// Regular, Bold or Fill.
        IconVariant Variant = IconVariant::Regular;
    };

    /// Displays one icon from Carbon::Icons in a square of `Size` points.
    void Icon(std::string_view icon, const IconOptions& options = {});
} // namespace Carbon
