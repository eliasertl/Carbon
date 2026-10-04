#include "Carbon/Widgets/Toggle.h"

#include <algorithm>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Widgets/WidgetInternal.h"

namespace Carbon
{
    namespace
    {
        // Track of a switch per control size; the knob is a circle inset by KnobInset.
        Vec2 GetSwitchSize(ControlSize size)
        {
            switch (size)
            {
                case ControlSize::Small:
                    return Vec2(30.0f, 18.0f);
                case ControlSize::Regular:
                    return Vec2(38.0f, 22.0f);
                case ControlSize::Large:
                    return Vec2(46.0f, 26.0f);
            }
            return Vec2(38.0f, 22.0f);
        }

        float GetCheckboxSize(ControlSize size)
        {
            switch (size)
            {
                case ControlSize::Small:
                    return 12.0f;
                case ControlSize::Regular:
                    return 14.0f;
                case ControlSize::Large:
                    return 16.0f;
            }
            return 14.0f;
        }

        constexpr float KnobInset = 2.0f;
        constexpr float SwitchGap = 10.0f;
        constexpr float CheckboxGap = 6.0f;

        // The knob glides with a touch of bounce, as on macOS.
        constexpr AnimationSpec KnobSpring = AnimationSpec::Spring(0.28f, 0.82f);
        constexpr AnimationSpec CheckSpring = AnimationSpec::Spring(0.18f).AsAppearance();
    } // namespace

    bool Toggle(std::string_view label, bool* value, const ToggleOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(value != nullptr, "Toggle needs a value to bind to");
        if (value == nullptr)
            return false;

        const ID id = GetID(label);
        const std::string_view title = GetDisplayLabel(label);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        const Vec2 titleSize = title.empty() ? Vec2() : MeasureText(title, spec);
        const bool isSwitch = options.Kind == ToggleKind::Switch;

        const Vec2 controlSize =
            isSwitch ? GetSwitchSize(options.ControlSize) : Vec2(GetCheckboxSize(options.ControlSize));
        const float gap = title.empty() ? 0.0f : (isSwitch ? SwitchGap : CheckboxGap);
        const Vec2 fit(titleSize.X + gap + controlSize.X, std::max(titleSize.Y, controlSize.Y));

        PushDisabled(options.Disabled);

        ItemOptions item;
        item.Width = options.Width;
        const Rect rect = AllocateItem(fit, item);
        const Interaction interaction = ButtonBehavior(id, rect);
        bool changed = false;
        if (interaction.Clicked)
        {
            *value = !*value;
            changed = true;
        }

        const Color labelColor = context.Style.GetColor(StyleColor::Label);
        const Color accent = Resolve(options.Tint, StyleColor::Accent);
        const float centerY = rect.GetCenter().Y;
        const float press = Animate(HashID("##press", id), interaction.Pressed ? 1.0f : 0.0f, CheckSpring);

        if (isSwitch)
        {
            // Label leading, switch trailing: with a stretched width they sit at opposite edges.
            const Rect track(rect.GetRight() - controlSize.X, centerY - controlSize.Y * 0.5f, controlSize.X,
                             controlSize.Y);
            const float on = Animate(HashID("##on", id), *value ? 1.0f : 0.0f, KnobSpring);
            const float amount = std::clamp(on, 0.0f, 1.0f);

            const Color off = Blend(context.Style.GetColor(StyleColor::ControlFill), labelColor, 0.08f);
            Color trackColor = Lerp(off, accent, amount);
            trackColor = Blend(trackColor, labelColor, context.Style.GetVar(StyleVar::PressedAmount) * press);
            context.Draw.AddSquircle(track, trackColor, track.Height * 0.5f, 0.0f);

            const float knobSize = track.Height - KnobInset * 2.0f;
            const float travel = track.Width - knobSize - KnobInset * 2.0f;
            const Rect knob(track.X + KnobInset + travel * on, track.Y + KnobInset, knobSize, knobSize);
            context.Draw.AddShadow(knob, context.Style.GetColor(StyleColor::Shadow), knobSize * 0.5f, 2.0f,
                                   Vec2(0.0f, 1.0f), 0.0f);
            context.Draw.AddCircle(knob.GetCenter(), knobSize * 0.5f, context.Style.GetColor(StyleColor::Knob));

            if (!title.empty())
                Internal::DrawLabel(context.Draw, rect, rect.X, title, spec, labelColor);
            DrawFocusRing(id, track, track.Height * 0.5f);
        }
        else
        {
            const Rect box(rect.X, centerY - controlSize.Y * 0.5f, controlSize.X, controlSize.Y);
            const bool isFilled = *value || options.IsMixed;
            const float on = Animate(HashID("##on", id), isFilled ? 1.0f : 0.0f, CheckSpring);
            const float radius = controlSize.X * 0.26f;

            // Off: a bordered well. On: filled with the accent color, with a white mark.
            Color fill = Lerp(context.Style.GetColor(StyleColor::ControlBackground), accent, on);
            fill = Blend(fill, labelColor, context.Style.GetVar(StyleVar::PressedAmount) * press);
            context.Draw.AddSquircle(box, fill, radius);
            const Color border = context.Style.GetColor(StyleColor::ControlBorder).WithOpacity(1.0f - on);
            context.Draw.AddSquircleStroke(box, border, radius, context.Style.GetVar(StyleVar::BorderWidth));

            const Color mark = context.Style.GetColor(StyleColor::OnAccent).WithOpacity(on);
            const float lineWidth = controlSize.X * 0.125f;
            const auto at = [&](float x, float y) { return Vec2(box.X + box.Width * x, box.Y + box.Height * y); };
            if (options.IsMixed)
            {
                context.Draw.AddLine(at(0.27f, 0.5f), at(0.73f, 0.5f), mark, lineWidth);
            }
            else
            {
                context.Draw.AddLine(at(0.25f, 0.52f), at(0.42f, 0.69f), mark, lineWidth);
                context.Draw.AddLine(at(0.42f, 0.69f), at(0.76f, 0.31f), mark, lineWidth);
            }

            if (!title.empty())
                Internal::DrawLabel(context.Draw, rect, box.GetRight() + CheckboxGap, title, spec, labelColor);
            DrawFocusRing(id, box, radius);
        }

        PopDisabled();
        return changed;
    }
} // namespace Carbon
