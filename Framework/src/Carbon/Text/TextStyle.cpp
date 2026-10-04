#include "Carbon/Text/TextStyle.h"

#include <array>
#include <cstddef>

namespace Carbon
{
    namespace
    {
        struct TextStyleEntry
        {
            float Size;
            float LineHeight;
            FontWeight Weight;
            FontWeight EmphasizedWeight;
        };

        // The macOS built-in text styles from the HIG's Typography page (point size / line height, weight,
        // emphasized weight). Public Sans has nearly the same cap height and x-height as SF Pro, so the sizes are
        // used unchanged; see Docs/Styling.md.
        constexpr std::array<TextStyleEntry, static_cast<size_t>(TextStyle::Count)> TextStyles = {{
            {26.0f, 32.0f, FontWeight::Regular, FontWeight::Bold},     // LargeTitle
            {22.0f, 26.0f, FontWeight::Regular, FontWeight::Bold},     // Title1
            {17.0f, 22.0f, FontWeight::Regular, FontWeight::Bold},     // Title2
            {15.0f, 20.0f, FontWeight::Regular, FontWeight::Semibold}, // Title3
            {13.0f, 16.0f, FontWeight::Bold, FontWeight::Heavy},       // Headline
            {13.0f, 16.0f, FontWeight::Regular, FontWeight::Semibold}, // Body
            {12.0f, 15.0f, FontWeight::Regular, FontWeight::Semibold}, // Callout
            {11.0f, 14.0f, FontWeight::Regular, FontWeight::Semibold}, // Subheadline
            {10.0f, 13.0f, FontWeight::Regular, FontWeight::Semibold}, // Footnote
            {10.0f, 13.0f, FontWeight::Regular, FontWeight::Medium},   // Caption1
            {10.0f, 13.0f, FontWeight::Medium, FontWeight::Semibold},  // Caption2
        }};
    } // namespace

    TextSpec GetTextSpec(TextStyle style, bool emphasized)
    {
        const size_t index =
            style < TextStyle::Count ? static_cast<size_t>(style) : static_cast<size_t>(TextStyle::Body);
        const TextStyleEntry& entry = TextStyles[index];
        TextSpec spec;
        spec.Size = entry.Size;
        spec.LineHeight = entry.LineHeight;
        spec.Weight = emphasized ? entry.EmphasizedWeight : entry.Weight;
        return spec;
    }
} // namespace Carbon
