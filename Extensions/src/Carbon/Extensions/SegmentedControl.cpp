#include "Carbon/Extensions/SegmentedControl.h"

#include <algorithm>
#include <cmath>

namespace Carbon
{
    namespace
    {
        // Space between the track's edge and the selected segment.
        constexpr float TrackInset = 2.0f;
        constexpr float SegmentPadding = 12.0f;
        // The selection glides with a touch of bounce, like the knob of a switch.
        constexpr AnimationSpec SelectionSpring = AnimationSpec::Spring(0.3f, 0.85f);
    } // namespace

    bool SegmentedControl(std::string_view label, int* selected, std::span<const std::string_view> segments,
                          const SegmentedControlOptions& options)
    {
        CB_VERIFY(selected != nullptr, "SegmentedControl needs a selection to bind to");
        if (selected == nullptr || segments.empty())
            return false;

        const ID id = GetID(label);
        const int count = static_cast<int>(segments.size());
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        DrawList& drawList = GetDrawList();

        float widest = 0.0f;
        for (const std::string_view segment : segments)
            widest = std::max(widest, MeasureText(segment, spec).X);
        const float fitWidth = (widest + SegmentPadding * 2.0f) * static_cast<float>(count) + TrackInset * 2.0f;

        PushDisabled(options.Disabled);

        ItemOptions item;
        item.Width = options.Width;
        const Rect rect = AllocateItem(Vec2(fitWidth, metrics.Height), item);
        const float segmentWidth = (rect.Width - TrackInset * 2.0f) / static_cast<float>(count);
        const auto getSegmentRect = [&rect, segmentWidth](float index)
        {
            return Rect(rect.X + TrackInset + segmentWidth * index, rect.Y + TrackInset, segmentWidth,
                        rect.Height - TrackInset * 2.0f);
        };

        bool changed = false;
        const int before = std::clamp(*selected, 0, count - 1);
        int current = before;

        // The whole control is one stop for Tab; the arrow keys move the selection.
        RegisterFocusable(id, rect);
        Interaction summary;
        summary.Hovered = IsRectHovered(rect);
        for (int i = 0; i < count; i++)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            behavior.ActivateOnPress = true;
            const Interaction interaction =
                ButtonBehavior(HashID(i, id), getSegmentRect(static_cast<float>(i)), behavior);
            summary.Pressed = summary.Pressed || interaction.Pressed;
            if (interaction.Clicked)
            {
                current = i;
                SetFocus(id);
            }
        }
        if (IsFocused(id) && !IsDisabled())
        {
            if (IsKeyPressed(Key::RightArrow))
                current = std::min(current + 1, count - 1);
            if (IsKeyPressed(Key::LeftArrow))
                current = std::max(current - 1, 0);
        }
        if (current != *selected)
        {
            *selected = current;
            changed = current != before;
        }
        summary.Focused = IsFocused(id);
        summary.FocusVisible = IsFocusVisible(id);
        summary.Clicked = changed;

        // Track.
        const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
        drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlFill), metrics.CornerRadius, smoothing);

        // The selected segment is a raised plate that slides to its place.
        const float position = Animate(HashID("##selection", id), static_cast<float>(current), SelectionSpring);
        const Rect plate = getSegmentRect(position);
        const float plateRadius = std::max(metrics.CornerRadius - TrackInset, 0.0f);
        const Color plateColor = Blend(GetStyleColor(StyleColor::Knob), GetStyleColor(StyleColor::Background),
                                       GetTheme().IsDark ? 0.6f : 0.0f);
        drawList.AddShadow(plate, GetStyleColor(StyleColor::Shadow).WithOpacity(0.6f), plateRadius, 3.0f,
                           Vec2(0.0f, 1.0f), smoothing);
        drawList.AddSquircle(plate, plateColor, plateRadius, smoothing);

        // Hairlines separate the segments, except next to the plate.
        const float pixel = GetContentScale().GetPixelSize();
        const Color separator = GetStyleColor(StyleColor::Separator);
        for (int i = 1; i < count; i++)
        {
            const float distance =
                std::min(std::abs(position - static_cast<float>(i)), std::abs(position + 1.0f - static_cast<float>(i)));
            const float opacity = std::clamp(distance * 2.0f, 0.0f, 1.0f);
            const float x = GetContentScale().Snap(rect.X + TrackInset + segmentWidth * static_cast<float>(i));
            drawList.AddRect(Rect(x, rect.Y + rect.Height * 0.28f, pixel, rect.Height * 0.44f),
                             separator.WithOpacity(opacity));
        }

        const Color labelColor = GetStyleColor(StyleColor::Label);
        for (int i = 0; i < count; i++)
        {
            const std::string_view segment = segments[static_cast<size_t>(i)];
            const Rect segmentRect = getSegmentRect(static_cast<float>(i));
            const float width = MeasureText(segment, spec).X;
            DrawLabel(drawList, segmentRect, segmentRect.GetCenter().X - width * 0.5f, segment, spec, labelColor);
        }

        DrawFocusRing(id, rect, metrics.CornerRadius);
        SetLastItem(id, rect, summary);
        PopDisabled();
        return changed;
    }

    bool SegmentedControl(std::string_view label, int* selected, std::initializer_list<std::string_view> segments,
                          const SegmentedControlOptions& options)
    {
        return SegmentedControl(label, selected, std::span<const std::string_view>(segments.begin(), segments.size()),
                                options);
    }
} // namespace Carbon
