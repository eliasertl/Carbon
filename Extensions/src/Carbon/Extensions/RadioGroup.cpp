#include "Carbon/Extensions/RadioGroup.h"

#include <algorithm>

namespace Carbon
{
    namespace
    {
        // The same sizes as a checkbox, which a radio button sits next to in forms.
        float GetButtonSize(ControlSize size)
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

        // A radio group is meant for two to five choices; longer lists belong in a pop-up button.
        constexpr int MaxItems = 64;
        constexpr float LabelGap = 6.0f;
        constexpr float VerticalSpacing = 6.0f;
        constexpr float HorizontalSpacing = 20.0f;
        constexpr AnimationSpec DotSpring = AnimationSpec::Spring(0.18f).AsAppearance();
    } // namespace

    bool RadioGroup(std::string_view label, int* selected, std::span<const std::string_view> items,
                    const RadioGroupOptions& options)
    {
        CB_VERIFY(selected != nullptr, "RadioGroup needs a selection to bind to");
        if (selected == nullptr || items.empty())
            return false;

        CB_VERIFY(items.size() <= MaxItems, "A RadioGroup has at most {} items; use a PopUpButton for more", MaxItems);
        const ID id = GetID(label);
        const int count = static_cast<int>(std::min<size_t>(items.size(), MaxItems));
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        const float buttonSize = GetButtonSize(options.ControlSize);
        const bool isHorizontal = options.Orientation == Axis::Horizontal;
        const float spacing = options.Spacing.value_or(isHorizontal ? HorizontalSpacing : VerticalSpacing);
        DrawList& drawList = GetDrawList();

        // Every button is as wide as the widest, so that a horizontal group is evenly spaced (HIG).
        float widest = 0.0f;
        float lineHeight = buttonSize;
        for (int i = 0; i < count; i++)
        {
            const Vec2 size = MeasureText(items[static_cast<size_t>(i)], spec);
            widest = std::max(widest, size.X);
            lineHeight = std::max(lineHeight, size.Y);
        }
        const Vec2 itemSize(buttonSize + LabelGap + widest, lineHeight);
        const float gaps = spacing * static_cast<float>(count - 1);
        const Vec2 fit = isHorizontal ? Vec2(itemSize.X * static_cast<float>(count) + gaps, itemSize.Y)
                                      : Vec2(itemSize.X, itemSize.Y * static_cast<float>(count) + gaps);

        PushDisabled(options.Disabled);
        const Rect rect = AllocateItem(fit);
        const auto getItemRect = [&](int index)
        {
            const float offset = static_cast<float>(index) * ((isHorizontal ? itemSize.X : itemSize.Y) + spacing);
            return isHorizontal ? Rect(rect.X + offset, rect.Y, itemSize.X, itemSize.Y)
                                : Rect(rect.X, rect.Y + offset, itemSize.X, itemSize.Y);
        };

        // -1 means nothing is selected; anything else outside the range is the nearest button.
        const int before = *selected == -1 ? -1 : std::clamp(*selected, 0, count - 1);
        int current = before;

        // The group is one stop for Tab; the arrow keys move the selection, as in AppKit.
        RegisterFocusable(id, rect);
        Interaction summary;
        summary.Hovered = IsRectHovered(rect);
        Interaction interactions[MaxItems];
        for (int i = 0; i < count; i++)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            interactions[i] = ButtonBehavior(HashID(i, id), getItemRect(i), behavior);
            summary.Pressed = summary.Pressed || interactions[i].Pressed;
            if (interactions[i].Clicked)
            {
                current = i;
                SetFocus(id);
            }
        }
        if (IsFocused(id) && !IsDisabled())
        {
            const bool next = IsKeyPressed(Key::DownArrow) || IsKeyPressed(Key::RightArrow);
            const bool previous = IsKeyPressed(Key::UpArrow) || IsKeyPressed(Key::LeftArrow);
            if (current < 0 && (next || previous || IsKeyPressed(Key::Space, false)))
                current = 0;
            else if (next)
                current = std::min(current + 1, count - 1);
            else if (previous)
                current = std::max(current - 1, 0);
        }
        bool changed = false;
        if (current != *selected)
        {
            *selected = current;
            changed = current != before;
        }
        summary.Focused = IsFocused(id);
        summary.FocusVisible = IsFocusVisible(id);
        summary.Clicked = changed;

        // Off: a bordered well. On: filled with the accent color around a white dot, like a checkbox's check.
        const Color labelColor = GetStyleColor(StyleColor::Label);
        const Color accent = GetStyleColor(StyleColor::Accent);
        const Color well = GetStyleColor(StyleColor::ControlBackground);
        const Color border = GetStyleColor(StyleColor::ControlBorder);
        const Color dot = GetStyleColor(StyleColor::OnAccent);
        const float borderWidth = GetStyleVar(StyleVar::BorderWidth);
        Rect focusCircle;
        for (int i = 0; i < count; i++)
        {
            const Rect itemRect = getItemRect(i);
            const Rect circle(itemRect.X, itemRect.GetCenter().Y - buttonSize * 0.5f, buttonSize, buttonSize);
            const float radius = buttonSize * 0.5f;
            const float on = Animate(HashID("##on", HashID(i, id)), i == current ? 1.0f : 0.0f, DotSpring);
            const ControlFeedback feedback =
                AnimateFeedback(HashID(i, id), interactions[i].Hovered, interactions[i].Pressed);

            drawList.AddSquircle(circle, ApplyFeedback(Lerp(well, accent, on), labelColor, feedback), radius, 0.0f);
            drawList.AddSquircleStroke(circle, border.WithOpacity(1.0f - on), radius, borderWidth, 0.0f);
            if (on > 0.001f)
                drawList.AddCircle(circle.GetCenter(), buttonSize * 0.2f * (0.5f + 0.5f * on), dot.WithOpacity(on));
            DrawLabel(drawList, itemRect, circle.GetRight() + LabelGap, items[static_cast<size_t>(i)], spec,
                      labelColor);
            if (i == std::max(current, 0))
                focusCircle = circle;
        }

        // The ring goes around the button the arrow keys act on.
        DrawFocusRing(id, focusCircle, buttonSize * 0.5f);
        SetLastItem(id, rect, summary);
        PopDisabled();
        return changed;
    }

    bool RadioGroup(std::string_view label, int* selected, std::initializer_list<std::string_view> items,
                    const RadioGroupOptions& options)
    {
        return RadioGroup(label, selected, std::span<const std::string_view>(items.begin(), items.size()), options);
    }
} // namespace Carbon
