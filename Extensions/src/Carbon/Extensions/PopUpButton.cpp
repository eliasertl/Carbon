#include "Carbon/Extensions/PopUpButton.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Extensions/Menu.h"

namespace Carbon
{
    namespace
    {
        // Geometry of the menu (see Menu.cpp) that lets the menu's current item sit exactly over the button.
        constexpr float MenuPadding = 5.0f;
        constexpr float MenuRowHeight = 22.0f;
        constexpr float MenuLabelInset = 5.0f + 8.0f + 18.0f;
        constexpr float IndicatorGap = 6.0f;

        struct PopUpState
        {
            /// Counts down after the menu was opened with the keyboard; at 1 its current item gets the highlight.
            /// That is one frame after the key press, which the item would otherwise take for itself.
            int FocusCountdown;
        };
    } // namespace

    bool PopUpButton(std::string_view label, int* selected, std::span<const std::string_view> items,
                     const PopUpButtonOptions& options)
    {
        CB_VERIFY(selected != nullptr, "PopUpButton needs a selection to bind to");
        if (selected == nullptr || items.empty())
            return false;

        const ID id = GetID(label);
        const ID menu = HashID("##menu", id);
        const int count = static_cast<int>(items.size());
        const int current = std::clamp(*selected, 0, count - 1);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        DrawList& drawList = GetDrawList();

        float widest = 0.0f;
        for (const std::string_view item : items)
            widest = std::max(widest, MeasureText(item, spec).X);
        // The indicator is a square with a small margin to the button's edge.
        const float indicatorInset = 3.0f;
        const float indicatorSize = metrics.Height - indicatorInset * 2.0f;

        PushDisabled(options.Disabled);

        ItemOptions item;
        item.Width = options.Width;
        const Rect rect = AllocateItem(
            Vec2(metrics.Padding + widest + IndicatorGap + indicatorSize + indicatorInset, metrics.Height), item);

        // Like on macOS the menu opens on the press, not the release.
        ButtonBehaviorOptions behavior;
        behavior.ActivateOnPress = true;
        const Interaction interaction = ButtonBehavior(id, rect, behavior);
        PopUpState& state = *GetState<PopUpState>(id);
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
        DrawLabel(drawList, rect, rect.X + metrics.Padding, items[static_cast<size_t>(current)], spec, labelColor);

        // The accent-colored square with the two chevrons says: this opens a list of choices.
        const Rect indicator(rect.GetRight() - indicatorInset - indicatorSize, rect.Y + indicatorInset, indicatorSize,
                             indicatorSize);
        drawList.AddSquircle(indicator, GetStyleColor(StyleColor::Accent),
                             std::max(metrics.CornerRadius - indicatorInset, 0.0f), smoothing);
        DrawIcon(drawList, indicator.GetCenter(), Icons::CaretUpDown, std::round(indicatorSize * 0.72f),
                 GetStyleColor(StyleColor::OnAccent), IconVariant::Bold);
        DrawFocusRing(id, rect, metrics.CornerRadius);
        PopDisabled();

        // The menu: its current item lies over the button, label over label.
        bool changed = false;
        MenuOptions menuOptions;
        const Vec2 origin(
            rect.X + metrics.Padding - MenuLabelInset,
            rect.Y + (rect.Height - MenuRowHeight) * 0.5f - MenuPadding - MenuRowHeight * static_cast<float>(current));
        menuOptions.Anchor = Rect(origin, Vec2());
        menuOptions.Gap = 0.0f;
        menuOptions.MinWidth = rect.GetRight() - origin.X + MenuPadding;
        if (BeginMenu(menu, menuOptions))
        {
            for (int i = 0; i < count; i++)
            {
                PushID(i);
                const std::string_view title = items[static_cast<size_t>(i)];
                if (state.FocusCountdown == 1 && i == current)
                    SetFocus(GetID(title), true);
                MenuItemOptions itemOptions;
                itemOptions.IsChecked = i == current;
                if (MenuItem(title, itemOptions) && i != current)
                {
                    *selected = i;
                    changed = true;
                }
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
        summary.Clicked = changed;
        SetLastItem(id, rect, summary);
        return changed;
    }

    bool PopUpButton(std::string_view label, int* selected, std::initializer_list<std::string_view> items,
                     const PopUpButtonOptions& options)
    {
        return PopUpButton(label, selected, std::span<const std::string_view>(items.begin(), items.size()), options);
    }
} // namespace Carbon
