#include "Carbon/Widgets/Slider.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/Layout.h"
#include "Carbon/Style/Style.h"
#include "Carbon/Widgets/ControlFeedback.h"

namespace Carbon
{
    namespace
    {
        constexpr float TrackThickness = 4.0f;
        // Without a step, the arrow keys move the knob by this fraction of the track.
        constexpr double KeyboardFraction = 0.05;
        // How much the knob's radius grows under the pointer.
        constexpr float KnobHoverGrowth = 1.0f;

        float GetKnobSize(ControlSize size)
        {
            switch (size)
            {
                case ControlSize::Small:
                    return 14.0f;
                case ControlSize::Regular:
                    return 18.0f;
                case ControlSize::Large:
                    return 22.0f;
            }
            return 18.0f;
        }

        // Where a value sits on the track, from 0 at `min` to 1 at `max`, and back.
        struct SliderRange
        {
            double Min = 0.0;
            double Max = 1.0;
            bool IsLogarithmic = false;

            double ToFraction(double value) const
            {
                double fraction = 0.0;
                if (IsLogarithmic)
                    fraction = std::log(std::max(value, Min) / Min) / std::log(Max / Min);
                else
                    fraction = (value - Min) / (Max - Min);
                return std::clamp(fraction, 0.0, 1.0);
            }

            double ToValue(double fraction) const
            {
                if (IsLogarithmic)
                    return Min * std::pow(Max / Min, fraction);
                return Min + (Max - Min) * fraction;
            }
        };

        double Quantize(double value, const SliderRange& range, double step)
        {
            if (step > 0.0)
                value = range.Min + std::round((value - range.Min) / step) * step;
            return std::clamp(value, range.Min, range.Max);
        }

        // The slider, for every type of value. `step` is already at least 1 for an int.
        bool EditSlider(std::string_view label, double& value, const SliderRange& range, double step,
                        const SliderOptions& options)
        {
            Context& context = Internal::GetFrameContext();
            const ID id = GetID(label);
            const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
            const float knobSize = GetKnobSize(options.ControlSize);
            const double initial = value;
            const bool isVertical = options.Axis == Axis::Vertical;

            PushDisabled(options.Disabled);

            // The slider's length runs along its axis; across it, it is as thick as a control.
            ItemOptions item;
            Vec2 size(knobSize * 3.0f, metrics.Height);
            if (isVertical)
            {
                item.Height = options.Height;
                size = Vec2(metrics.Height, knobSize * 3.0f);
            }
            else
            {
                item.Width = options.Width;
            }
            const Rect rect = AllocateItem(size, item);

            // The knob's center travels between two points: leading to trailing, or bottom to top.
            const float length = std::max((isVertical ? rect.Height : rect.Width) - knobSize, 1.0f);
            const Vec2 center = rect.GetCenter();
            const Vec2 trackStart = isVertical ? Vec2(center.X, rect.GetBottom() - knobSize * 0.5f)
                                               : Vec2(rect.X + knobSize * 0.5f, center.Y);
            const Vec2 direction = isVertical ? Vec2(0.0f, -1.0f) : Vec2(1.0f, 0.0f);
            const auto toPoint = [&](double fraction) { return trackStart + direction * (length * float(fraction)); };
            // How far along the track a position is, in points.
            const auto toDistance = [&](Vec2 position)
            { return isVertical ? trackStart.Y - position.Y : position.X - trackStart.X; };

            const DragInteraction drag = DragBehavior(id, rect);
            if (drag.Started)
            {
                // Grabbing the knob keeps the grab point under the pointer; clicking the track moves the knob there.
                const float distance = toDistance(drag.Position) - length * float(range.ToFraction(value));
                *GetState<float>(HashID("##grab", id)) = std::abs(distance) <= knobSize * 0.5f ? distance : 0.0f;
            }
            if (drag.Active)
            {
                const float grab = *GetState<float>(HashID("##grab", id));
                const double fraction = std::clamp(double(toDistance(drag.Position) - grab) / length, 0.0, 1.0);
                value = Quantize(range.ToValue(fraction), range, step);
            }

            if (drag.Focused)
            {
                const auto move = [&](int steps)
                {
                    if (step > 0.0)
                        value = Quantize(value + step * steps, range, step);
                    else
                        value = range.ToValue(std::clamp(range.ToFraction(value) + KeyboardFraction * steps, 0.0, 1.0));
                };
                if (IsKeyPressed(Key::RightArrow) || IsKeyPressed(Key::UpArrow))
                    move(1);
                if (IsKeyPressed(Key::LeftArrow) || IsKeyPressed(Key::DownArrow))
                    move(-1);
                if (IsKeyPressed(Key::Home, false))
                    value = range.Min;
                if (IsKeyPressed(Key::End, false))
                    value = range.Max;
            }

            const Vec2 knobCenter = toPoint(range.ToFraction(value));
            const Vec2 trackEnd = toPoint(1.0);
            const Color labelColor = context.Style.GetColor(StyleColor::Label);

            // Track: a pill, filled with the accent color from the minimum up to the knob.
            const Color trackColor = Blend(context.Style.GetColor(StyleColor::ControlFill), labelColor, 0.06f);
            context.Draw.AddLine(trackStart, trackEnd, trackColor, TrackThickness);
            context.Draw.AddLine(trackStart, knobCenter, Resolve(options.Tint, StyleColor::Accent), TrackThickness);

            if (options.ShowsTicks && step > 0.0)
            {
                // Below a horizontal track, at the trailing side of a vertical one.
                const int count = static_cast<int>(std::round((range.Max - range.Min) / step));
                const Color tick = context.Style.GetColor(StyleColor::TertiaryLabel);
                const Vec2 offset =
                    isVertical ? Vec2(knobSize * 0.5f + 3.0f, 0.0f) : Vec2(0.0f, knobSize * 0.5f + 3.0f);
                for (int i = 0; i <= count && count <= 100; i++)
                {
                    const double tickValue = std::min(range.Min + step * i, range.Max);
                    context.Draw.AddCircle(toPoint(range.ToFraction(tickValue)) + offset, 1.0f, tick);
                }
            }

            // Knob: a white circle with a soft shadow that darkens slightly while held.
            // It grows a little under the pointer, so that it is clear what a click will grab.
            const ControlFeedback feedback = AnimateFeedback(id, drag.Hovered || drag.Active, drag.Active);
            const float knobRadius = knobSize * 0.5f + KnobHoverGrowth * feedback.Hover;
            const Rect knob = Rect::FromCenter(knobCenter, Vec2(knobRadius * 2.0f));
            context.Draw.AddShadow(knob, context.Style.GetColor(StyleColor::Shadow), knobRadius, 3.0f, Vec2(0.0f, 1.0f),
                                   0.0f);
            context.Draw.AddCircle(
                knobCenter, knobRadius,
                Blend(context.Style.GetColor(StyleColor::Knob), Color::Black(), 0.08f * feedback.Press));
            context.Draw.AddCircleStroke(knobCenter, knobRadius, Color::Black().WithAlpha(0.1f), 0.5f);
            DrawFocusRing(id, knob, knobRadius);

            PopDisabled();
            return value != initial;
        }

        // Checks the arguments that every overload takes; reports misuse and returns false for it.
        bool CheckSlider(const void* value, double min, double max, const SliderOptions& options)
        {
            CB_VERIFY(value != nullptr, "Slider needs a value to bind to");
            CB_VERIFY(max > min, "Slider range is empty: min {} must be less than max {}", min, max);
            const bool isLogarithmic = options.Scale == SliderScale::Logarithmic;
            CB_VERIFY(!isLogarithmic || min > 0.0,
                      "A logarithmic slider needs a positive range, but min is {}; use a min above 0", min);
            CB_VERIFY(options.Step >= 0.0, "Slider step must not be negative; it is {}", options.Step);
            return value != nullptr && max > min && (!isLogarithmic || min > 0.0) && options.Step >= 0.0;
        }

        SliderRange MakeRange(double min, double max, const SliderOptions& options)
        {
            return SliderRange{min, max, options.Scale == SliderScale::Logarithmic};
        }
    } // namespace

    bool Slider(std::string_view label, float* value, float min, float max, const SliderOptions& options)
    {
        Internal::GetFrameContext();
        if (!CheckSlider(value, min, max, options))
            return false;
        double edited = *value;
        if (!EditSlider(label, edited, MakeRange(min, max, options), options.Step, options))
            return false;
        const float result = static_cast<float>(edited);
        const bool changed = result != *value;
        *value = result;
        return changed;
    }

    bool Slider(std::string_view label, double* value, double min, double max, const SliderOptions& options)
    {
        Internal::GetFrameContext();
        if (!CheckSlider(value, min, max, options))
            return false;
        return EditSlider(label, *value, MakeRange(min, max, options), options.Step, options);
    }

    bool Slider(std::string_view label, int* value, int min, int max, const SliderOptions& options)
    {
        Internal::GetFrameContext();
        if (!CheckSlider(value, min, max, options))
            return false;
        // Whole steps of at least 1, so every value the knob reaches is an int.
        const double step = std::max(1.0, std::round(options.Step));
        double edited = *value;
        if (!EditSlider(label, edited, MakeRange(min, max, options), step, options))
            return false;
        const int result = static_cast<int>(std::lround(edited));
        const bool changed = result != *value;
        *value = result;
        return changed;
    }
} // namespace Carbon
