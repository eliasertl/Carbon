#pragma once

#include <array>
#include <cstddef>

#include "Carbon/Core/Color.h"
#include "Carbon/Style/StyleColor.h"
#include "Carbon/Style/StyleVar.h"
#include "Carbon/Text/Font.h"
#include "Carbon/Text/TextStyle.h"

namespace Carbon
{
    /// Size, line height and weights of one text style.
    struct TextStyleSpec
    {
        float Size = 13.0f;
        float LineHeight = 16.0f;
        FontWeight Weight = FontWeight::Regular;
        FontWeight EmphasizedWeight = FontWeight::Semibold;
    };

    /// The complete look of the interface: semantic colors, metrics and the type ramp. Start from Light() or
    /// Dark() and change what you need:
    ///
    ///     Carbon::Theme theme = Carbon::Theme::Dark();
    ///     theme.SetColor(Carbon::StyleColor::Accent, Carbon::Color::FromHex(0xFF9F0A));
    ///     Carbon::SetTheme(theme);
    struct Theme
    {
        /// Apple-like light surfaces with black text.
        static Theme Light();
        /// Pure black background (#000000) with white text, for OLED displays.
        static Theme Dark();

        Color GetColor(StyleColor color) const { return Colors[static_cast<size_t>(color)]; }
        void SetColor(StyleColor color, Color value) { Colors[static_cast<size_t>(color)] = value; }
        float GetVar(StyleVar var) const { return Vars[static_cast<size_t>(var)]; }
        void SetVar(StyleVar var, float value) { Vars[static_cast<size_t>(var)] = value; }
        const TextStyleSpec& GetTextStyle(TextStyle style) const { return TextStyles[static_cast<size_t>(style)]; }
        void SetTextStyle(TextStyle style, const TextStyleSpec& spec) { TextStyles[static_cast<size_t>(style)] = spec; }

        std::array<Color, static_cast<size_t>(StyleColor::Count)> Colors{};
        std::array<float, static_cast<size_t>(StyleVar::Count)> Vars{};
        std::array<TextStyleSpec, static_cast<size_t>(TextStyle::Count)> TextStyles{};
        /// The font of all text; null uses the embedded default (Public Sans).
        Carbon::Font* Font = nullptr;
        /// True for dark appearances. Lets components pick assets or adjust contrast.
        bool IsDark = false;
    };

    /// Blends two themes: colors and metrics are interpolated; the type ramp, font and appearance flag switch to
    /// `to` as soon as the blend starts. Used for animated theme switches.
    Theme Lerp(const Theme& from, const Theme& to, float t);
} // namespace Carbon
