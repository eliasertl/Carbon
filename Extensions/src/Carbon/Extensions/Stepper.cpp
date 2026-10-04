#include "Carbon/Extensions/Stepper.h"

#include <algorithm>
#include <cmath>

namespace Carbon
{
    namespace
    {
        float GetStepperWidth(ControlSize size)
        {
            switch (size)
            {
                case ControlSize::Small:
                    return 15.0f;
                case ControlSize::Regular:
                    return 18.0f;
                case ControlSize::Large:
                    return 22.0f;
            }
            return 18.0f;
        }

        double StepValue(double value, int direction, const StepperOptions& options)
        {
            const double low = std::min(options.Min, options.Max);
            const double high = std::max(options.Min, options.Max);
            const double next = value + options.Step * direction;
            if (options.Wraps)
            {
                if (next > high)
                    return low;
                if (next < low)
                    return high;
            }
            return std::clamp(next, low, high);
        }
    } // namespace

    bool Stepper(std::string_view label, double* value, const StepperOptions& options)
    {
        CB_VERIFY(value != nullptr, "Stepper needs a value to bind to");
        if (value == nullptr)
            return false;

        const ID id = GetID(label);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        DrawList& drawList = GetDrawList();

        PushDisabled(options.Disabled);

        const Rect rect = AllocateItem(Vec2(GetStepperWidth(options.ControlSize), metrics.Height));
        const Rect halves[2] = {Rect(rect.X, rect.Y, rect.Width, rect.Height * 0.5f),
                                Rect(rect.X, rect.Y + rect.Height * 0.5f, rect.Width, rect.Height * 0.5f)};
        const ID halfIDs[2] = {HashID("##up", id), HashID("##down", id)};

        // One stop for Tab; the halves are buttons for the mouse only and repeat while held.
        RegisterFocusable(id, rect);
        int direction = 0;
        Interaction interactions[2];
        for (int i = 0; i < 2; i++)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.ActivateOnPress = true;
            behavior.Repeat = true;
            interactions[i] = ButtonBehavior(halfIDs[i], halves[i], behavior);
            if (interactions[i].Clicked)
            {
                direction += i == 0 ? 1 : -1;
                SetFocus(id);
            }
        }
        if (IsFocused(id) && !IsDisabled())
        {
            if (IsKeyPressed(Key::UpArrow))
                direction += 1;
            if (IsKeyPressed(Key::DownArrow))
                direction -= 1;
        }

        bool changed = false;
        if (direction != 0)
        {
            const double next = StepValue(*value, direction, options);
            changed = next != *value;
            *value = next;
        }

        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        const Color labelColor = GetStyleColor(StyleColor::Label);
        drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlFill), metrics.CornerRadius, smoothing);

        // The half under the pointer darkens. It is clipped to its half of the control's shape.
        const std::string_view icons[2] = {Icons::CaretUp, Icons::CaretDown};
        const float iconSize = std::round(rect.Width * 0.55f);
        for (int i = 0; i < 2; i++)
        {
            const ControlFeedback feedback =
                AnimateFeedback(halfIDs[i], interactions[i].Hovered, interactions[i].Pressed);
            const float amount = GetStyleVar(StyleVar::HoverAmount) * feedback.Hover +
                                 GetStyleVar(StyleVar::PressedAmount) * feedback.Press;
            if (amount > 0.001f)
            {
                drawList.PushClipRect(halves[i]);
                drawList.AddSquircle(rect, labelColor.WithOpacity(amount), metrics.CornerRadius, smoothing);
                drawList.PopClipRect();
            }
            DrawIcon(drawList, halves[i].GetCenter(), icons[i], iconSize, labelColor, IconVariant::Bold);
        }

        DrawFocusRing(id, rect, metrics.CornerRadius);

        Interaction summary;
        summary.Hovered = interactions[0].Hovered || interactions[1].Hovered;
        summary.Pressed = interactions[0].Pressed || interactions[1].Pressed;
        summary.Clicked = changed;
        summary.Focused = IsFocused(id);
        summary.FocusVisible = IsFocusVisible(id);
        SetLastItem(id, rect, summary);
        PopDisabled();
        return changed;
    }

    bool Stepper(std::string_view label, int* value, const StepperOptions& options)
    {
        CB_VERIFY(value != nullptr, "Stepper needs a value to bind to");
        if (value == nullptr)
            return false;

        double number = static_cast<double>(*value);
        const bool changed = Stepper(label, &number, options);
        if (changed)
            *value = static_cast<int>(std::lround(number));
        return changed;
    }
} // namespace Carbon
