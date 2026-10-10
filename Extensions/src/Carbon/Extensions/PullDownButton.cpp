#include "Carbon/Extensions/PullDownButton.h"

#include <algorithm>

#include "Carbon/Extensions/Menu.h"

namespace Carbon
{
    namespace
    {
        constexpr float IconSize = 14.0f;
        constexpr float IconGap = 5.0f;
        constexpr float ChevronSize = 10.0f;
        constexpr float ChevronGap = 6.0f;

        struct PullDownState
        {
            /// The menu was opened with the keyboard: its first item gets the highlight.
            bool FocusFirst;
        };
    } // namespace

    bool BeginPullDownButton(std::string_view label, const PullDownButtonOptions& options)
    {
        const ID id = GetID(label);
        const ID menu = HashID("##menu", id);
        const std::string_view title = GetDisplayLabel(label);
        const ControlMetrics metrics = GetControlMetrics(options.ControlSize);
        const TextSpec spec = GetTextSpec(metrics.Style);
        DrawList& drawList = GetDrawList();

        const float titleWidth = title.empty() ? 0.0f : MeasureText(title, spec).X;
        float contentWidth = titleWidth + ChevronGap + ChevronSize;
        if (!options.Icon.empty())
            contentWidth += IconSize + (title.empty() ? 0.0f : IconGap);

        PushDisabled(options.Disabled);

        ItemOptions item;
        item.Width = options.Width;
        const Rect rect = AllocateItem(Vec2(contentWidth + metrics.Padding * 2.0f, metrics.Height), item);

        ButtonBehaviorOptions behavior;
        behavior.ActivateOnPress = true;
        const Interaction interaction = ButtonBehavior(id, rect, behavior);
        PullDownState& state = *GetState<PullDownState>(id);
        const bool isArrowPressed = interaction.Focused && IsKeyPressed(Key::DownArrow, false);
        if ((interaction.Clicked || isArrowPressed) && !IsOverlayOpen(menu))
        {
            OpenOverlay(menu);
            state.FocusFirst = !IsMousePressed() && !IsMouseReleased();
        }

        const ControlFeedback feedback = AnimateFeedback(id, interaction.Hovered, interaction.Pressed);
        const Color labelColor = GetStyleColor(StyleColor::Label);
        drawList.AddSquircle(rect, ApplyFeedback(GetStyleColor(StyleColor::ControlFill), labelColor, feedback),
                             metrics.CornerRadius, GetStyleVar(StyleVar::CornerSmoothing));

        const float centerY = rect.GetCenter().Y;
        float x = rect.X + metrics.Padding;
        if (!options.Icon.empty())
        {
            DrawIcon(drawList, Vec2(x + IconSize * 0.5f, centerY), options.Icon, IconSize, labelColor);
            x += IconSize + IconGap;
        }
        if (!title.empty())
            DrawLabel(drawList, rect, x, title, spec, labelColor);
        DrawIcon(drawList, Vec2(rect.GetRight() - metrics.Padding - ChevronSize * 0.5f, centerY), Icons::CaretDown,
                 ChevronSize, GetStyleColor(StyleColor::SecondaryLabel), IconVariant::Bold);
        DrawFocusRing(id, rect, metrics.CornerRadius);
        PopDisabled();
        SetLastItem(id, rect, interaction);

        MenuOptions menuOptions;
        menuOptions.Anchor = rect;
        menuOptions.MinWidth = rect.Width;
        if (!BeginMenu(menu, menuOptions))
            return false;
        if (state.FocusFirst)
        {
            FocusNext();
            state.FocusFirst = false;
        }
        return true;
    }

    void EndPullDownButton()
    {
        EndMenu();
    }
} // namespace Carbon
