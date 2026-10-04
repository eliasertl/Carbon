#include "Carbon/Extensions/ColorWell.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <iterator>
#include <string>

namespace Carbon
{
    namespace
    {
        constexpr float WellWidth = 44.0f;
        constexpr float WellInset = 3.0f;
        constexpr float SwatchSize = 20.0f;
        constexpr float SwatchGap = 4.0f;
        constexpr int SwatchesPerRow = 8;
        constexpr float PopoverPadding = 12.0f;
        constexpr float PopoverWidth =
            SwatchSize * SwatchesPerRow + SwatchGap * (SwatchesPerRow - 1) + PopoverPadding * 2.0f;

        // The text of the hex field while it is being edited. Only one color well's popover is open at a time.
        std::string s_HexText;

        int ToByte(float value)
        {
            return static_cast<int>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }

        void FormatHex(const Color& color, std::string& text)
        {
            text.clear();
            std::format_to(std::back_inserter(text), "#{:02X}{:02X}{:02X}", ToByte(color.R), ToByte(color.G),
                           ToByte(color.B));
        }

        // Accepts "RRGGBB" with or without a leading '#'.
        bool ParseHex(std::string_view text, Color* color)
        {
            if (!text.empty() && text.front() == '#')
                text.remove_prefix(1);
            if (text.size() != 6)
                return false;
            uint32_t value = 0;
            for (const char c : text)
            {
                uint32_t digit = 0;
                if (c >= '0' && c <= '9')
                    digit = static_cast<uint32_t>(c - '0');
                else if (c >= 'a' && c <= 'f')
                    digit = static_cast<uint32_t>(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    digit = static_cast<uint32_t>(c - 'A' + 10);
                else
                    return false;
                value = value * 16 + digit;
            }
            *color = Color::FromHex(value, color->A);
            return true;
        }

        bool IsSameColor(const Color& a, const Color& b)
        {
            return ToByte(a.R) == ToByte(b.R) && ToByte(a.G) == ToByte(b.G) && ToByte(a.B) == ToByte(b.B);
        }

        // One round palette entry. Returns true when it was chosen.
        bool PaletteSwatch(int index, const Color& swatch, bool isCurrent)
        {
            const ID id = GetID(index);
            const Rect rect = AllocateItem(Vec2(SwatchSize, SwatchSize));
            const Interaction interaction = ButtonBehavior(id, rect);
            const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);

            DrawList& drawList = GetDrawList();
            const Vec2 center = rect.GetCenter();
            const float radius = SwatchSize * 0.5f - 1.0f + feedback.Hover - feedback.Press;
            drawList.AddCircle(center, radius, swatch);
            // A hairline keeps white and black visible on either background.
            drawList.AddCircleStroke(center, radius, GetStyleColor(StyleColor::ControlBorder),
                                     GetContentScale().GetPixelSize());
            if (isCurrent)
            {
                const float luminance = swatch.R * 0.299f + swatch.G * 0.587f + swatch.B * 0.114f;
                DrawIcon(drawList, center, Icons::Check, 12.0f, luminance > 0.6f ? Color::Black() : Color::White(),
                         IconVariant::Bold);
            }
            DrawFocusRing(id, rect, SwatchSize * 0.5f);
            return interaction.Clicked;
        }

        // A labeled slider for one channel, shown as a whole number up to `scale`.
        bool ChannelSlider(std::string_view title, std::string_view id, float* channel, float scale)
        {
            bool changed = false;
            BeginHStack({.Spacing = 6.0f, .Width = Size::Fill()});
            Text(title, {.Secondary = true, .Width = 10.0f});

            float value = std::round(std::clamp(*channel, 0.0f, 1.0f) * scale);
            SliderOptions slider;
            slider.Step = 1.0f;
            slider.ControlSize = ControlSize::Small;
            slider.Width = Size::Fill();
            if (Slider(id, &value, 0.0f, scale, slider))
            {
                *channel = value / scale;
                changed = true;
            }

            char number[8] = {};
            const auto end = std::format_to_n(number, std::size(number) - 1, "{}", static_cast<int>(value)).out;
            Text(std::string_view(number, static_cast<size_t>(end - number)),
                 {.Secondary = true, .Width = 24.0f, .Alignment = TextAlignment::Trailing});
            EndHStack();
            return changed;
        }
    } // namespace

    bool ColorWell(std::string_view label, Color* color, const ColorWellOptions& options)
    {
        CB_VERIFY(color != nullptr, "ColorWell needs a color to bind to");
        if (color == nullptr)
            return false;

        const ID id = GetID(label);
        const ID popover = HashID("##popover", id);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        DrawList& drawList = GetDrawList();
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);

        PushDisabled(options.Disabled);
        const Rect rect = AllocateItem(Vec2(WellWidth, metrics.Height));
        const Interaction interaction = ButtonBehavior(id, rect);
        if (interaction.Clicked)
            OpenOverlay(popover);

        // The well: a control-colored frame around the color itself.
        const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);
        drawList.AddSquircle(
            rect, ApplyFeedback(GetStyleColor(StyleColor::ControlFill), GetStyleColor(StyleColor::Label), feedback),
            metrics.CornerRadius, smoothing);
        const Rect inner = rect.Expand(-WellInset);
        const float innerRadius = std::max(metrics.CornerRadius - WellInset, 0.0f);
        drawList.AddSquircle(inner, *color, innerRadius, smoothing);
        drawList.AddSquircleStroke(inner, GetStyleColor(StyleColor::ControlBorder), innerRadius,
                                   GetContentScale().GetPixelSize(), smoothing);
        DrawFocusRing(id, rect, metrics.CornerRadius);
        PopDisabled();

        bool changed = false;
        OverlayOptions overlay;
        overlay.Anchor = rect;
        overlay.Alignment = Alignment::Center;
        overlay.Gap = 2.0f;
        overlay.ShowsArrow = true;
        overlay.Padding = EdgeInsets(PopoverPadding);
        overlay.Spacing = 8.0f;
        overlay.Width = Size::Fixed(PopoverWidth);
        if (BeginOverlay(popover, overlay))
        {
            const StyleColor palette[SwatchesPerRow * 2] = {
                StyleColor::Red,    StyleColor::Orange, StyleColor::Yellow, StyleColor::Green,
                StyleColor::Mint,   StyleColor::Teal,   StyleColor::Cyan,   StyleColor::Blue,
                StyleColor::Indigo, StyleColor::Purple, StyleColor::Pink,   StyleColor::Brown,
                StyleColor::Gray,   StyleColor::Count,  StyleColor::Count,  StyleColor::Count};
            // The last three entries are not theme colors: a dark gray, black and white.
            const Color neutrals[3] = {Color::FromHex(0x48484A), Color::Black(), Color::White()};

            for (int row = 0; row < 2; row++)
            {
                BeginHStack({.Spacing = SwatchGap});
                for (int column = 0; column < SwatchesPerRow; column++)
                {
                    const int index = row * SwatchesPerRow + column;
                    const Color swatch = palette[index] == StyleColor::Count
                                             ? neutrals[index - (SwatchesPerRow * 2 - 3)]
                                             : GetTargetTheme().GetColor(palette[index]);
                    if (PaletteSwatch(index, swatch, IsSameColor(swatch, *color)))
                    {
                        *color = swatch.WithAlpha(color->A);
                        changed = true;
                    }
                }
                EndHStack();
            }

            Separator();
            changed = ChannelSlider("R", "##red", &color->R, 255.0f) || changed;
            changed = ChannelSlider("G", "##green", &color->G, 255.0f) || changed;
            changed = ChannelSlider("B", "##blue", &color->B, 255.0f) || changed;
            if (options.ShowsOpacity)
                changed = ChannelSlider("A", "##opacity", &color->A, 100.0f) || changed;

            // The hex field shows the color unless it is being typed into.
            BeginHStack({.Spacing = 6.0f, .Width = Size::Fill()});
            Text("Hex", {.Secondary = true});
            if (!IsFocused(GetID("##hex")))
                FormatHex(*color, s_HexText);
            TextFieldOptions field;
            field.Width = Size::Fill();
            field.ControlSize = ControlSize::Small;
            field.MaxLength = 7;
            if (TextField("##hex", &s_HexText, field) && ParseHex(s_HexText, color))
                changed = true;
            EndHStack();

            EndOverlay();
        }

        Interaction summary = interaction;
        summary.Clicked = changed;
        SetLastItem(id, rect, summary);
        return changed;
    }
} // namespace Carbon
