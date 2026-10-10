#include "Carbon/Extensions/PathControl.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Extensions/Menu.h"

namespace Carbon
{
    namespace
    {
        constexpr int MaxComponents = 64;
        // Space between the field's edge and the first and last component.
        constexpr float FieldInset = 3.0f;
        // Space between a component's highlight and its content, and between its icon and label.
        constexpr float ComponentPadding = 5.0f;
        constexpr float IconGap = 4.0f;
        constexpr float ChevronWidth = 12.0f;
        // A name is shown only when at least this much of it fits; otherwise the component shows its icon.
        constexpr float MinimumLabelWidth = 14.0f;
        constexpr AnimationSpec WidthSpring = AnimationSpec::Spring(0.25f);

        // Geometry of the menu (see Menu.cpp) that lets the menu's first item sit over the button.
        constexpr float MenuPadding = 5.0f;
        constexpr float MenuRowHeight = 22.0f;
        constexpr float MenuLabelInset = 5.0f + 8.0f + 18.0f;
        constexpr float IndicatorGap = 6.0f;

        struct PathControlState
        {
            /// The component the keyboard acts on; valid while HasHighlight.
            int Highlight;
            bool HasHighlight;
            /// The component under the pointer in the previous frame, or -1. Its name is never hidden.
            int Hovered;
            /// As in PopUpButton: after the menu was opened with the keyboard, its first item gets the highlight.
            int FocusCountdown;
        };

        float GetIconSize(const TextSpec& spec)
        {
            return std::round(spec.Size * 1.2f);
        }

        int StandardPathControl(ID id, std::span<const PathControlItem> path, int count,
                                const PathControlOptions& options)
        {
            const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
            const TextSpec spec = GetTextSpec(metrics.Style);
            const float iconSize = GetIconSize(spec);
            DrawList& drawList = GetDrawList();
            PathControlState& state = *GetState<PathControlState>(id);

            // Widths of each component with its name and with only its icon (or an ellipsis, without an icon).
            float full[MaxComponents];
            float collapsed[MaxComponents];
            const float ellipsis = MeasureText("\xE2\x80\xA6", spec).X;
            float fitWidth = FieldInset * 2.0f + ChevronWidth * static_cast<float>(count - 1);
            for (int i = 0; i < count; i++)
            {
                const PathControlItem& item = path[static_cast<size_t>(i)];
                const bool hasIcon = !item.Icon.empty();
                const float labelWidth = item.Label.empty() ? 0.0f : MeasureText(item.Label, spec).X;
                const float iconWidth = hasIcon ? iconSize + (labelWidth > 0.0f ? IconGap : 0.0f) : 0.0f;
                full[i] = ComponentPadding * 2.0f + iconWidth + labelWidth;
                collapsed[i] = std::min(full[i], ComponentPadding * 2.0f + (hasIcon ? iconSize : ellipsis));
                fitWidth += full[i];
            }

            PushDisabled(options.Disabled);
            ItemOptions itemOptions;
            itemOptions.Width = options.Width;
            const Rect rect = AllocateItem(Vec2(fitWidth, metrics.Height), itemOptions);

            // The keyboard starts at the selected item, the end of the path.
            if (!state.HasHighlight || state.Highlight >= count)
                state.Highlight = count - 1;
            const bool isFocused = IsFocused(id);
            if (!isFocused)
                state.HasHighlight = false;
            const int expanded = state.Hovered >= 0 && state.Hovered < count
                                     ? state.Hovered
                                     : (isFocused && state.HasHighlight ? state.Highlight : -1);

            // Too narrow: hide the names between the first and the last component, starting next to the root
            // (HIG). If that is not enough, truncate the root's name, then the selected item's, which matters most.
            float target[MaxComponents];
            float total = 0.0f;
            for (int i = 0; i < count; i++)
            {
                target[i] = full[i];
                total += full[i];
            }
            const float available = rect.Width - FieldInset * 2.0f - ChevronWidth * static_cast<float>(count - 1);
            for (int i = 1; i < count - 1 && total > available; i++)
            {
                if (i == expanded)
                    continue;
                total -= target[i] - collapsed[i];
                target[i] = collapsed[i];
            }
            const int truncationOrder[2] = {0, count - 1};
            for (const int i : truncationOrder)
            {
                if (total <= available || (i == 0 && count == 1))
                    continue;
                const float cut = std::min(target[i] - collapsed[i], total - available);
                target[i] -= cut;
                total -= cut;
            }

            // Components glide to their widths, so a name that appears under the pointer pushes the others aside.
            const ID widthID = HashID("##width", id);
            Rect components[MaxComponents];
            float x = rect.X + FieldInset;
            for (int i = 0; i < count; i++)
            {
                const float width = std::max(Animate(HashID(i, widthID), target[i], WidthSpring), 0.0f);
                // Even as icons, a long path can be wider than a very narrow field: what does not fit is cut off.
                components[i] = Rect(x, rect.Y + 2.0f, width, rect.Height - 4.0f).GetIntersection(rect);
                x += width + ChevronWidth;
            }

            // One stop for Tab; the arrow keys move a highlight along the path, Space and Enter activate it.
            RegisterFocusable(id, rect);
            Interaction summary;
            summary.Hovered = IsRectHovered(rect);
            int activated = -1;
            int hovered = -1;
            Interaction interactions[MaxComponents];
            for (int i = 0; i < count; i++)
            {
                ButtonBehaviorOptions behavior;
                behavior.Focusable = false;
                interactions[i] = ButtonBehavior(HashID(i, id), components[i], behavior);
                summary.Pressed = summary.Pressed || interactions[i].Pressed;
                // A finger cannot hover: pressing a component shows its name as hovering does.
                if (interactions[i].Hovered || interactions[i].Pressed)
                    hovered = i;
                if (interactions[i].Clicked)
                {
                    activated = i;
                    state.Highlight = i;
                    state.HasHighlight = true;
                    SetFocus(id);
                }
            }
            state.Hovered = hovered;
            if (isFocused && !IsDisabled())
            {
                const int before = state.Highlight;
                if (IsKeyPressed(Key::LeftArrow))
                    state.Highlight = std::max(state.Highlight - 1, 0);
                if (IsKeyPressed(Key::RightArrow))
                    state.Highlight = std::min(state.Highlight + 1, count - 1);
                if (IsKeyPressed(Key::Home, false))
                    state.Highlight = 0;
                if (IsKeyPressed(Key::End, false))
                    state.Highlight = count - 1;
                if (state.Highlight != before)
                    state.HasHighlight = true;
                if (IsKeyPressed(Key::Space, false) || IsKeyPressed(Key::Enter, false) ||
                    IsKeyPressed(Key::KeypadEnter, false))
                {
                    activated = state.Highlight;
                    state.HasHighlight = true;
                }
            }

            // Drawing: a field, then each component with its hover highlight, icon and name, then the chevrons.
            const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
            const Color labelColor = GetStyleColor(StyleColor::Label);
            const Color secondary = GetStyleColor(StyleColor::SecondaryLabel);
            const Color accent = GetStyleColor(StyleColor::Accent);
            const float highlightRadius = std::max(metrics.CornerRadius - 2.0f, 0.0f);
            drawList.AddSquircle(rect, GetStyleColor(StyleColor::ControlFill), metrics.CornerRadius, smoothing);
            for (int i = 0; i < count; i++)
            {
                const PathControlItem& item = path[static_cast<size_t>(i)];
                const Rect& component = components[i];
                const ControlFeedback feedback =
                    AnimateFeedback(HashID(i, id), interactions[i].Hovered, interactions[i].Pressed);
                drawList.AddSquircle(component, ApplyFeedback(labelColor.WithOpacity(0.0f), labelColor, feedback),
                                     highlightRadius, smoothing);

                drawList.PushClipRect(component);
                float contentX = component.X + ComponentPadding;
                if (!item.Icon.empty())
                {
                    DrawIcon(drawList, Vec2(contentX + iconSize * 0.5f, component.GetCenter().Y), item.Icon, iconSize,
                             accent);
                    contentX += iconSize + IconGap;
                }
                const float labelWidth = component.GetRight() - ComponentPadding - contentX;
                if (!item.Label.empty() && (labelWidth >= MinimumLabelWidth || item.Icon.empty()))
                {
                    TextSpec labelSpec = spec;
                    // The slack absorbs pixel snapping: a name is truncated only when its component is narrower.
                    labelSpec.MaxWidth = std::max(labelWidth + 1.0f, 1.0f);
                    labelSpec.Wraps = false;
                    DrawLabel(drawList, component, contentX, item.Label, labelSpec,
                              i == count - 1 ? labelColor : secondary);
                }
                drawList.PopClipRect();

                if (i < count - 1 && component.GetRight() + ChevronWidth <= rect.GetRight())
                {
                    DrawIcon(drawList, Vec2(component.GetRight() + ChevronWidth * 0.5f, component.GetCenter().Y),
                             Icons::CaretRight, std::round(spec.Size * 0.85f), GetStyleColor(StyleColor::TertiaryLabel),
                             IconVariant::Bold);
                }
            }

            const Rect& ringRect = components[std::clamp(state.Highlight, 0, count - 1)];
            DrawFocusRing(id, ringRect, highlightRadius);
            summary.Focused = IsFocused(id);
            summary.FocusVisible = IsFocusVisible(id);
            summary.Clicked = activated >= 0;
            SetLastItem(id, rect, summary);
            PopDisabled();
            return activated;
        }

        int PopUpPathControl(ID id, std::span<const PathControlItem> path, int count, const PathControlOptions& options)
        {
            const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
            const TextSpec spec = GetTextSpec(metrics.Style);
            const float iconSize = GetIconSize(spec);
            const ID menu = HashID("##menu", id);
            DrawList& drawList = GetDrawList();
            PathControlState& state = *GetState<PathControlState>(id);

            // The button shows the selected item: the end of the path.
            const PathControlItem& last = path[static_cast<size_t>(count - 1)];
            const float iconWidth = last.Icon.empty() ? 0.0f : iconSize + IconGap;
            const float indicatorInset = 3.0f;
            const float indicatorSize = metrics.Height - indicatorInset * 2.0f;
            const float fitWidth = metrics.Padding + iconWidth + MeasureText(last.Label, spec).X + IndicatorGap +
                                   indicatorSize + indicatorInset;

            PushDisabled(options.Disabled);
            ItemOptions itemOptions;
            itemOptions.Width = options.Width;
            const Rect rect = AllocateItem(Vec2(fitWidth, metrics.Height), itemOptions);

            ButtonBehaviorOptions behavior;
            behavior.ActivateOnPress = true;
            const Interaction interaction = ButtonBehavior(id, rect, behavior);
            const bool isArrowPressed =
                interaction.Focused && (IsKeyPressed(Key::DownArrow, false) || IsKeyPressed(Key::UpArrow, false));
            if ((interaction.Clicked || isArrowPressed) && !IsOverlayOpen(menu))
            {
                OpenOverlay(menu);
                state.FocusCountdown = IsMousePressed() ? 0 : 2;
            }

            const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);
            const Color labelColor = GetStyleColor(StyleColor::Label);
            const float smoothing = GetStyleVar(StyleVar::CornerSmoothing);
            drawList.AddSquircle(rect, ApplyFeedback(GetStyleColor(StyleColor::ControlFill), labelColor, feedback),
                                 metrics.CornerRadius, smoothing);
            float contentX = rect.X + metrics.Padding;
            if (!last.Icon.empty())
            {
                DrawIcon(drawList, Vec2(contentX + iconSize * 0.5f, rect.GetCenter().Y), last.Icon, iconSize,
                         GetStyleColor(StyleColor::Accent));
                contentX += iconSize + IconGap;
            }
            const Rect indicator(rect.GetRight() - indicatorInset - indicatorSize, rect.Y + indicatorInset,
                                 indicatorSize, indicatorSize);
            TextSpec labelSpec = spec;
            // Truncated only when the button is narrower than its label; the slack absorbs pixel snapping.
            labelSpec.MaxWidth = std::max(indicator.X - IndicatorGap - contentX + 1.0f, 1.0f);
            labelSpec.Wraps = false;
            DrawLabel(drawList, rect, contentX, last.Label, labelSpec, labelColor);
            drawList.AddSquircle(indicator, GetStyleColor(StyleColor::Accent),
                                 std::max(metrics.CornerRadius - indicatorInset, 0.0f), smoothing);
            DrawIcon(drawList, indicator.GetCenter(), Icons::CaretUpDown, std::round(indicatorSize * 0.72f),
                     GetStyleColor(StyleColor::OnAccent), IconVariant::Bold);
            DrawFocusRing(id, rect, metrics.CornerRadius);
            PopDisabled();

            // The menu lists the path from the selected item up to the root, the selected item over the button.
            int activated = -1;
            MenuOptions menuOptions;
            const Vec2 origin(contentX - MenuLabelInset,
                              rect.Y + (rect.Height - GetAdaptiveRowHeight(MenuRowHeight)) * 0.5f - MenuPadding);
            menuOptions.Anchor = Rect(origin, Vec2());
            menuOptions.Gap = 0.0f;
            menuOptions.MinWidth = rect.GetRight() - origin.X + MenuPadding;
            if (BeginMenu(menu, menuOptions))
            {
                for (int i = count - 1; i >= 0; i--)
                {
                    const PathControlItem& item = path[static_cast<size_t>(i)];
                    PushID(i);
                    if (state.FocusCountdown == 1 && i == count - 1)
                        SetFocus(GetID(item.Label), true);
                    MenuItemOptions menuItem;
                    menuItem.Icon = item.Icon;
                    if (MenuItem(item.Label, menuItem))
                        activated = i;
                    PopID();
                }
                if (state.FocusCountdown > 0)
                {
                    state.FocusCountdown--;
                    RequestAnimationFrame();
                }
                EndMenu();
            }

            Interaction summary = interaction;
            summary.Clicked = activated >= 0;
            SetLastItem(id, rect, summary);
            return activated;
        }
    } // namespace

    int PathControl(std::string_view label, std::span<const PathControlItem> path, const PathControlOptions& options)
    {
        CB_VERIFY(path.size() <= MaxComponents, "A PathControl shows at most {} components", MaxComponents);
        const ID id = GetID(label);
        const int count = static_cast<int>(std::min<size_t>(path.size(), MaxComponents));
        if (count == 0)
        {
            // Nothing to show, but the control keeps its place.
            ItemOptions itemOptions;
            itemOptions.Width = options.Width;
            const Rect rect = AllocateItem(Vec2(0.0f, GetControlMetrics(options.ControlSize).Height), itemOptions);
            SetLastItem(id, rect, Interaction());
            return -1;
        }
        return options.Style == PathControlStyle::PopUp ? PopUpPathControl(id, path, count, options)
                                                        : StandardPathControl(id, path, count, options);
    }
} // namespace Carbon
