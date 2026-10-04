#include "Carbon/Style/Theme.h"

#include "Carbon/Core/Math.h"
#include "Carbon/Draw/DrawTypes.h"

namespace Carbon
{
    namespace
    {
        // Metrics and the type ramp are shared by both appearances.
        void SetSharedValues(Theme& theme)
        {
            theme.SetVar(StyleVar::CornerRadius, 6.0f);
            theme.SetVar(StyleVar::CornerSmoothing, DefaultCornerSmoothing);
            theme.SetVar(StyleVar::GroupCornerRadius, 10.0f);
            theme.SetVar(StyleVar::OverlayCornerRadius, 10.0f);
            theme.SetVar(StyleVar::Spacing, 8.0f);
            theme.SetVar(StyleVar::ControlHeight, 24.0f);
            theme.SetVar(StyleVar::ControlPadding, 10.0f);
            theme.SetVar(StyleVar::BorderWidth, 1.0f);
            theme.SetVar(StyleVar::FocusRingWidth, 3.0f);
            theme.SetVar(StyleVar::FocusRingOffset, 1.0f);
            theme.SetVar(StyleVar::DisabledOpacity, 0.4f);
            theme.SetVar(StyleVar::HoverAmount, 0.05f);
            theme.SetVar(StyleVar::PressedAmount, 0.12f);
            theme.SetVar(StyleVar::ScrollIndicatorWidth, 7.0f);

            // The macOS built-in text styles from the HIG's Typography page. Public Sans has the same x-height
            // as SF Pro, so the sizes are used unchanged; see Docs/Styling.md.
            theme.SetTextStyle(TextStyle::LargeTitle, {26.0f, 32.0f, FontWeight::Regular, FontWeight::Bold});
            theme.SetTextStyle(TextStyle::Title1, {22.0f, 26.0f, FontWeight::Regular, FontWeight::Bold});
            theme.SetTextStyle(TextStyle::Title2, {17.0f, 22.0f, FontWeight::Regular, FontWeight::Bold});
            theme.SetTextStyle(TextStyle::Title3, {15.0f, 20.0f, FontWeight::Regular, FontWeight::Semibold});
            theme.SetTextStyle(TextStyle::Headline, {13.0f, 16.0f, FontWeight::Bold, FontWeight::Heavy});
            theme.SetTextStyle(TextStyle::Body, {13.0f, 16.0f, FontWeight::Regular, FontWeight::Semibold});
            theme.SetTextStyle(TextStyle::Callout, {12.0f, 15.0f, FontWeight::Regular, FontWeight::Semibold});
            theme.SetTextStyle(TextStyle::Subheadline, {11.0f, 14.0f, FontWeight::Regular, FontWeight::Semibold});
            theme.SetTextStyle(TextStyle::Footnote, {10.0f, 13.0f, FontWeight::Regular, FontWeight::Semibold});
            theme.SetTextStyle(TextStyle::Caption1, {10.0f, 13.0f, FontWeight::Regular, FontWeight::Medium});
            theme.SetTextStyle(TextStyle::Caption2, {10.0f, 13.0f, FontWeight::Medium, FontWeight::Semibold});
        }
    } // namespace

    Theme Theme::Light()
    {
        Theme theme;
        theme.IsDark = false;
        SetSharedValues(theme);

        theme.SetColor(StyleColor::Background, Color::FromHex(0xFFFFFF));
        theme.SetColor(StyleColor::SecondaryBackground, Color::FromHex(0xF2F2F7));
        theme.SetColor(StyleColor::TertiaryBackground, Color::FromHex(0xFFFFFF));

        theme.SetColor(StyleColor::Label, Color::FromHex(0x000000));
        theme.SetColor(StyleColor::SecondaryLabel, Color::FromHex(0x6E6E73));
        theme.SetColor(StyleColor::TertiaryLabel, Color::FromHex(0xAEAEB2));
        theme.SetColor(StyleColor::QuaternaryLabel, Color::FromHex(0xD1D1D6));
        theme.SetColor(StyleColor::Separator, Color::FromHex(0xDCDCE0));

        theme.SetColor(StyleColor::ControlBackground, Color::FromHex(0xFFFFFF));
        theme.SetColor(StyleColor::ControlFill, Color::FromHex(0xE9E9EB));
        theme.SetColor(StyleColor::ControlBorder, Color::FromHex(0xD1D1D6));
        theme.SetColor(StyleColor::Knob, Color::FromHex(0xFFFFFF));

        theme.SetColor(StyleColor::Accent, Color::FromHex(0x007AFF));
        theme.SetColor(StyleColor::OnAccent, Color::FromHex(0xFFFFFF));
        theme.SetColor(StyleColor::Selection, Color::FromHex(0x007AFF));
        theme.SetColor(StyleColor::UnemphasizedSelection, Color::FromHex(0xDCDCDC));
        theme.SetColor(StyleColor::TextSelection, Color::FromHex(0xB3D7FF));
        theme.SetColor(StyleColor::Destructive, Color::FromHex(0xFF3B30));
        theme.SetColor(StyleColor::FocusRing, Color::FromHex(0x007AFF, 0.5f));

        theme.SetColor(StyleColor::OverlayBackground, Color::FromHex(0xFFFFFF));
        theme.SetColor(StyleColor::OverlayBorder, Color::FromHex(0x000000, 0.12f));
        theme.SetColor(StyleColor::Scrim, Color::FromHex(0x000000, 0.2f));
        theme.SetColor(StyleColor::Shadow, Color::FromHex(0x000000, 0.2f));
        theme.SetColor(StyleColor::ScrollIndicator, Color::FromHex(0x000000, 0.35f));

        theme.SetColor(StyleColor::Red, Color::FromHex(0xFF3B30));
        theme.SetColor(StyleColor::Orange, Color::FromHex(0xFF9500));
        theme.SetColor(StyleColor::Yellow, Color::FromHex(0xFFCC00));
        theme.SetColor(StyleColor::Green, Color::FromHex(0x34C759));
        theme.SetColor(StyleColor::Mint, Color::FromHex(0x00C7BE));
        theme.SetColor(StyleColor::Teal, Color::FromHex(0x30B0C7));
        theme.SetColor(StyleColor::Cyan, Color::FromHex(0x32ADE6));
        theme.SetColor(StyleColor::Blue, Color::FromHex(0x007AFF));
        theme.SetColor(StyleColor::Indigo, Color::FromHex(0x5856D6));
        theme.SetColor(StyleColor::Purple, Color::FromHex(0xAF52DE));
        theme.SetColor(StyleColor::Pink, Color::FromHex(0xFF2D55));
        theme.SetColor(StyleColor::Brown, Color::FromHex(0xA2845E));
        theme.SetColor(StyleColor::Gray, Color::FromHex(0x8E8E93));
        return theme;
    }

    Theme Theme::Dark()
    {
        Theme theme;
        theme.IsDark = true;
        SetSharedValues(theme);

        theme.SetColor(StyleColor::Background, Color::FromHex(0x000000));
        theme.SetColor(StyleColor::SecondaryBackground, Color::FromHex(0x1C1C1E));
        theme.SetColor(StyleColor::TertiaryBackground, Color::FromHex(0x2C2C2E));

        theme.SetColor(StyleColor::Label, Color::FromHex(0xFFFFFF));
        theme.SetColor(StyleColor::SecondaryLabel, Color::FromHex(0x98989F));
        theme.SetColor(StyleColor::TertiaryLabel, Color::FromHex(0x636366));
        theme.SetColor(StyleColor::QuaternaryLabel, Color::FromHex(0x3A3A3C));
        theme.SetColor(StyleColor::Separator, Color::FromHex(0x38383A));

        theme.SetColor(StyleColor::ControlBackground, Color::FromHex(0x1C1C1E));
        theme.SetColor(StyleColor::ControlFill, Color::FromHex(0x2C2C2E));
        theme.SetColor(StyleColor::ControlBorder, Color::FromHex(0x48484A));
        theme.SetColor(StyleColor::Knob, Color::FromHex(0xFFFFFF));

        theme.SetColor(StyleColor::Accent, Color::FromHex(0x0A84FF));
        theme.SetColor(StyleColor::OnAccent, Color::FromHex(0xFFFFFF));
        theme.SetColor(StyleColor::Selection, Color::FromHex(0x0A84FF));
        theme.SetColor(StyleColor::UnemphasizedSelection, Color::FromHex(0x464646));
        theme.SetColor(StyleColor::TextSelection, Color::FromHex(0x3F638B));
        theme.SetColor(StyleColor::Destructive, Color::FromHex(0xFF453A));
        theme.SetColor(StyleColor::FocusRing, Color::FromHex(0x1A8FFF, 0.7f));

        theme.SetColor(StyleColor::OverlayBackground, Color::FromHex(0x1E1E20));
        theme.SetColor(StyleColor::OverlayBorder, Color::FromHex(0xFFFFFF, 0.16f));
        theme.SetColor(StyleColor::Scrim, Color::FromHex(0x000000, 0.5f));
        theme.SetColor(StyleColor::Shadow, Color::FromHex(0x000000, 0.5f));
        theme.SetColor(StyleColor::ScrollIndicator, Color::FromHex(0xFFFFFF, 0.4f));

        theme.SetColor(StyleColor::Red, Color::FromHex(0xFF453A));
        theme.SetColor(StyleColor::Orange, Color::FromHex(0xFF9F0A));
        theme.SetColor(StyleColor::Yellow, Color::FromHex(0xFFD60A));
        theme.SetColor(StyleColor::Green, Color::FromHex(0x30D158));
        theme.SetColor(StyleColor::Mint, Color::FromHex(0x63E6E2));
        theme.SetColor(StyleColor::Teal, Color::FromHex(0x40C8E0));
        theme.SetColor(StyleColor::Cyan, Color::FromHex(0x64D2FF));
        theme.SetColor(StyleColor::Blue, Color::FromHex(0x0A84FF));
        theme.SetColor(StyleColor::Indigo, Color::FromHex(0x5E5CE6));
        theme.SetColor(StyleColor::Purple, Color::FromHex(0xBF5AF2));
        theme.SetColor(StyleColor::Pink, Color::FromHex(0xFF375F));
        theme.SetColor(StyleColor::Brown, Color::FromHex(0xAC8E68));
        theme.SetColor(StyleColor::Gray, Color::FromHex(0x8E8E93));
        return theme;
    }

    Theme Lerp(const Theme& from, const Theme& to, float t)
    {
        if (t <= 0.0f)
            return from;
        if (t >= 1.0f)
            return to;

        // Interpolating the type ramp would rasterize every glyph at every intermediate size and weight, so
        // text metrics, font and the appearance flag take their new values right away.
        Theme result = to;
        for (size_t i = 0; i < result.Colors.size(); i++)
            result.Colors[i] = Lerp(from.Colors[i], to.Colors[i], t);
        for (size_t i = 0; i < result.Vars.size(); i++)
            result.Vars[i] = Lerp(from.Vars[i], to.Vars[i], t);
        return result;
    }
} // namespace Carbon
