#include "Carbon/Widgets/Button.h"

#include <algorithm>
#include <string>

#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Widgets/ControlFeedback.h"

namespace Carbon
{
    bool Button(std::string_view label, const ButtonOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const ID id = GetID(label);
        const std::string_view title = GetDisplayLabel(label);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);

        // Icon and title are drawn as one piece of text, so the icon aligns with the capitals.
        std::string& content = context.ScratchText;
        content.assign(options.Icon);
        if (!options.Icon.empty() && !title.empty())
            content.append("  ");
        content.append(title);
        const Vec2 contentSize = MeasureText(content, spec);

        PushDisabled(options.Disabled);

        ItemOptions item;
        item.Width = options.Width;
        const Rect rect = AllocateItem(Vec2(contentSize.X + metrics.Padding * 2.0f, metrics.Height), item);

        ButtonBehaviorOptions behavior;
        behavior.IsDefault = options.IsDefault;
        const Interaction interaction = ButtonBehavior(id, rect, behavior);
        const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);

        const Color labelColor = context.Style.GetColor(StyleColor::Label);
        const Color accent = Resolve(options.Tint, StyleColor::Accent);
        Color fill;
        Color text;
        switch (options.Role)
        {
            case ButtonRole::Default:
                fill = ApplyFeedback(context.Style.GetColor(StyleColor::ControlFill), labelColor, feedback);
                text = labelColor;
                break;
            case ButtonRole::Prominent:
                // An accent fill brightens on hover and darkens when pressed.
                fill = Blend(Blend(accent, Color::White(), 0.1f * feedback.Hover * (1.0f - feedback.Press)),
                             Color::Black(), 0.18f * feedback.Press);
                text = context.Style.GetColor(StyleColor::OnAccent);
                break;
            case ButtonRole::Plain:
                // Invisible at rest; the standard control fill appears with hover and press.
                fill = ApplyFeedback(context.Style.GetColor(StyleColor::ControlFill), labelColor, feedback)
                           .WithOpacity(std::max(feedback.Hover, feedback.Press));
                text = accent;
                break;
            case ButtonRole::Destructive:
            {
                const Color destructive = context.Style.GetColor(StyleColor::Destructive);
                const Color base = Blend(context.Style.GetColor(StyleColor::ControlFill), destructive, 0.12f);
                fill = ApplyFeedback(base, destructive, feedback);
                text = destructive;
                break;
            }
        }

        const float radius = options.CornerRadius.value_or(metrics.CornerRadius);
        const float smoothing = Resolve(options.CornerSmoothing, StyleVar::CornerSmoothing);
        context.Draw.AddSquircle(rect, fill, radius, smoothing);
        DrawLabel(context.Draw, rect, rect.GetCenter().X - contentSize.X * 0.5f, context.ScratchText, spec, text);
        DrawFocusRing(id, rect, radius);

        PopDisabled();
        return interaction.Clicked;
    }
} // namespace Carbon
