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
        // Without a step, the arrow keys move by this fraction of the range.
        constexpr float KeyboardFraction = 0.05f;

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

        float Quantize(float value, float min, float max, float step)
        {
            if (step > 0.0f)
                value = min + std::round((value - min) / step) * step;
            return std::clamp(value, min, max);
        }
    } // namespace

    bool Slider(std::string_view label, float* value, float min, float max, const SliderOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        CB_VERIFY(value != nullptr, "Slider needs a value to bind to");
        CB_VERIFY(max > min, "Slider range is empty: min {} must be less than max {}", min, max);
        if (value == nullptr || !(max > min))
            return false;

        const ID id = GetID(label);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const float knobSize = GetKnobSize(options.ControlSize);
        const float initial = *value;

        PushDisabled(options.Disabled);

        ItemOptions item;
        item.Width = options.Width;
        const Rect rect = AllocateItem(Vec2(knobSize * 3.0f, metrics.Height), item);

        // The knob's center travels between these two positions.
        const float trackStart = rect.X + knobSize * 0.5f;
        const float trackLength = std::max(rect.Width - knobSize, 1.0f);
        const float centerY = rect.GetCenter().Y;
        const auto toFraction = [&](float v) { return std::clamp((v - min) / (max - min), 0.0f, 1.0f); };

        const DragInteraction drag = DragBehavior(id, rect);
        if (drag.Started)
        {
            // Grabbing the knob keeps the grab point under the pointer; clicking the track moves the knob there.
            const float knobCenter = trackStart + trackLength * toFraction(*value);
            const float distance = drag.Position.X - knobCenter;
            *GetState<float>(HashID("##grab", id)) = std::abs(distance) <= knobSize * 0.5f ? distance : 0.0f;
        }
        if (drag.Active)
        {
            const float grab = *GetState<float>(HashID("##grab", id));
            const float fraction = std::clamp((drag.Position.X - grab - trackStart) / trackLength, 0.0f, 1.0f);
            *value = Quantize(min + (max - min) * fraction, min, max, options.Step);
        }

        if (drag.Focused)
        {
            const float step = options.Step > 0.0f ? options.Step : (max - min) * KeyboardFraction;
            if (IsKeyPressed(Key::RightArrow) || IsKeyPressed(Key::UpArrow))
                *value = Quantize(*value + step, min, max, options.Step);
            if (IsKeyPressed(Key::LeftArrow) || IsKeyPressed(Key::DownArrow))
                *value = Quantize(*value - step, min, max, options.Step);
            if (IsKeyPressed(Key::Home, false))
                *value = min;
            if (IsKeyPressed(Key::End, false))
                *value = max;
        }

        const float fraction = toFraction(*value);
        const Vec2 knobCenter(trackStart + trackLength * fraction, centerY);
        const Color labelColor = context.Style.GetColor(StyleColor::Label);

        // Track: a pill, filled with the accent color up to the knob.
        const Color trackColor = Blend(context.Style.GetColor(StyleColor::ControlFill), labelColor, 0.06f);
        context.Draw.AddLine(Vec2(trackStart, centerY), Vec2(trackStart + trackLength, centerY), trackColor,
                             TrackThickness);
        context.Draw.AddLine(Vec2(trackStart, centerY), knobCenter, Resolve(options.Tint, StyleColor::Accent),
                             TrackThickness);

        if (options.ShowsTicks && options.Step > 0.0f)
        {
            const int count = static_cast<int>(std::round((max - min) / options.Step));
            const Color tick = context.Style.GetColor(StyleColor::TertiaryLabel);
            for (int i = 0; i <= count && count <= 100; i++)
            {
                const float x = trackStart + trackLength * (static_cast<float>(i) / static_cast<float>(count));
                context.Draw.AddCircle(Vec2(x, centerY + knobSize * 0.5f + 3.0f), 1.0f, tick);
            }
        }

        // Knob: a white circle with a soft shadow that darkens slightly while held.
        const float press =
            Animate(HashID("##press", id), drag.Active ? 1.0f : 0.0f, AnimationSpec::Spring(0.12f).AsAppearance());
        const Rect knob = Rect::FromCenter(knobCenter, Vec2(knobSize));
        context.Draw.AddShadow(knob, context.Style.GetColor(StyleColor::Shadow), knobSize * 0.5f, 3.0f,
                               Vec2(0.0f, 1.0f), 0.0f);
        context.Draw.AddCircle(knobCenter, knobSize * 0.5f,
                               Blend(context.Style.GetColor(StyleColor::Knob), Color::Black(), 0.08f * press));
        context.Draw.AddCircleStroke(knobCenter, knobSize * 0.5f, Color::Black().WithAlpha(0.1f), 0.5f);
        DrawFocusRing(id, knob, knobSize * 0.5f);

        PopDisabled();
        return *value != initial;
    }
} // namespace Carbon
