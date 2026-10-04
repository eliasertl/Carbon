#pragma once

#include <cstdint>

#include "Carbon/Text/TextSpec.h"

namespace Carbon
{
    /// The macOS type ramp from Apple's Human Interface Guidelines.
    enum class TextStyle : uint8_t
    {
        LargeTitle,
        Title1,
        Title2,
        Title3,
        Headline,
        Body,
        Callout,
        Subheadline,
        Footnote,
        Caption1,
        Caption2,

        Count
    };

    /// Returns font, size, line height and weight of a text style in the current theme. `emphasized` selects the
    /// HIG's emphasized weight (for example bold for titles and semibold for body text).
    TextSpec GetTextSpec(TextStyle style, bool emphasized = false);
} // namespace Carbon
