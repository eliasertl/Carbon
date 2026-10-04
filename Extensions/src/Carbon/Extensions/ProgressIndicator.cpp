#include "Carbon/Extensions/ProgressIndicator.h"

#include <algorithm>
#include <cmath>

namespace Carbon
{
    namespace
    {
        constexpr float Pi = 3.14159265f;
        constexpr int SpokeCount = 8;
        // Seconds for the bright spoke to go around once, and for the sliding bar segment to cross the track.
        constexpr double SpinPeriod = 0.8;
        constexpr double SlidePeriod = 1.5;

        float GetBarThickness(ControlSize size)
        {
            switch (size)
            {
                case ControlSize::Small:
                    return 4.0f;
                case ControlSize::Regular:
                    return 6.0f;
                case ControlSize::Large:
                    return 8.0f;
            }
            return 6.0f;
        }

        float GetSpinnerSize(ControlSize size)
        {
            switch (size)
            {
                case ControlSize::Small:
                    return 12.0f;
                case ControlSize::Regular:
                    return 16.0f;
                case ControlSize::Large:
                    return 32.0f;
            }
            return 16.0f;
        }

        void SetStaticLastItem(const Rect& rect)
        {
            Interaction interaction;
            interaction.Hovered = IsRectHovered(rect);
            SetLastItem(ID(), rect, interaction);
        }

        void DrawBar(float value, const ProgressIndicatorOptions& options)
        {
            DrawList& drawList = GetDrawList();
            const float thickness = GetBarThickness(options.ControlSize);
            ItemOptions item;
            item.Width = options.Width;
            const Rect rect = AllocateItem(Vec2(180.0f, thickness), item);
            const float radius = thickness * 0.5f;
            const Color tint = Resolve(options.Tint, StyleColor::Accent);

            drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlFill), radius, 0.0f);
            if (options.IsIndeterminate)
            {
                // A segment a third of the track long crosses it again and again, easing in and out.
                const float phase = static_cast<float>(std::fmod(GetTime(), SlidePeriod) / SlidePeriod);
                const float length = rect.Width * 0.35f;
                const float start = rect.X - length + (rect.Width + length) * Ease(Easing::EaseInOut, phase);
                const float from = std::max(start, rect.X);
                const float to = std::min(start + length, rect.GetRight());
                if (to - from > 0.5f)
                    drawList.AddSquircle(Rect(from, rect.Y, to - from, rect.Height), tint, radius, 0.0f);
                RequestAnimationFrame();
            }
            else
            {
                const float filled = rect.Width * std::clamp(value, 0.0f, 1.0f);
                // Below its own thickness the fill would no longer be a pill.
                if (filled > 0.0f)
                    drawList.AddSquircle(Rect(rect.X, rect.Y, std::max(filled, thickness), rect.Height), tint, radius,
                                         0.0f);
            }
            SetStaticLastItem(rect);
        }

        void DrawSpinner(float value, const ProgressIndicatorOptions& options)
        {
            DrawList& drawList = GetDrawList();
            const float size = GetSpinnerSize(options.ControlSize);
            const Rect rect = AllocateItem(Vec2(size, size));
            const Vec2 center = rect.GetCenter();

            if (options.IsIndeterminate)
            {
                // Eight spokes; the brightest one steps around the circle and the others trail off behind it.
                const Color color = Resolve(options.Tint, StyleColor::SecondaryLabel);
                const int lead = static_cast<int>(std::fmod(GetTime(), SpinPeriod) / SpinPeriod * SpokeCount);
                const float inner = size * 0.24f;
                const float outer = size * 0.46f;
                const float width = std::max(size * 0.11f, 1.5f);
                for (int i = 0; i < SpokeCount; i++)
                {
                    const float angle = static_cast<float>(i) / SpokeCount * 2.0f * Pi - Pi * 0.5f;
                    const Vec2 direction(std::cos(angle), std::sin(angle));
                    const int age = ((lead - i) % SpokeCount + SpokeCount) % SpokeCount;
                    const float opacity = 1.0f - 0.8f * static_cast<float>(age) / (SpokeCount - 1);
                    drawList.AddLine(center + direction * inner, center + direction * (outer - width * 0.5f),
                                     color.WithOpacity(opacity), width);
                }
                RequestAnimationFrame();
            }
            else
            {
                // A ring that closes clockwise from the top. The arc is made of short segments.
                const float width = std::max(size * 0.14f, 2.0f);
                const float radius = size * 0.5f - width * 0.5f;
                drawList.AddCircleStroke(center, size * 0.5f, GetStyleColor(StyleColor::ControlFill), width);
                const float fraction = std::clamp(value, 0.0f, 1.0f);
                if (fraction > 0.0f)
                {
                    const Color tint = Resolve(options.Tint, StyleColor::Accent);
                    const int segments = std::max(static_cast<int>(std::ceil(fraction * radius * 2.0f * Pi / 2.0f)), 1);
                    Vec2 previous = center + Vec2(0.0f, -radius);
                    for (int i = 1; i <= segments; i++)
                    {
                        const float angle = fraction * 2.0f * Pi * static_cast<float>(i) / segments - Pi * 0.5f;
                        const Vec2 point = center + Vec2(std::cos(angle), std::sin(angle)) * radius;
                        drawList.AddLine(previous, point, tint, width);
                        previous = point;
                    }
                }
            }
            SetStaticLastItem(rect);
        }
    } // namespace

    void ProgressIndicator(float value, const ProgressIndicatorOptions& options)
    {
        if (options.Kind == ProgressKind::Bar)
            DrawBar(value, options);
        else
            DrawSpinner(value, options);
    }
} // namespace Carbon
