#include "Carbon/Extensions/Internal/NavigationStack.h"

#include <algorithm>

namespace Carbon::Internal
{
    namespace
    {
        constexpr int MaxDepth = 4;
        // The navigation bar of iOS: 44 points, the back button at the leading edge, the title in the middle.
        constexpr float BarHeight = 44.0f;
        constexpr float BarPadding = 8.0f;
        constexpr float BackIconSize = 20.0f;
        constexpr float BackGap = 2.0f;
        // While the detail slides in, the root moves along by this fraction of the width and darkens a little,
        // as in UINavigationController.
        constexpr float Parallax = 0.3f;
        constexpr float RootDimming = 0.1f;
        constexpr float EdgeShadowBlur = 12.0f;
        // A swipe back starts this close to the leading edge, and goes back when it ends past half the width or
        // faster than this, in points per second.
        constexpr float EdgeWidth = 20.0f;
        constexpr float PopVelocity = 500.0f;
        const AnimationSpec PushSpring = AnimationSpec::Spring(0.35f, 1.0f);

        // Remembered per navigation stack, also while it is shown side by side.
        struct NavigationState
        {
            bool IsDetailShown;
            bool IsSwiping;
            /// The progress the swipe has reached.
            float SwipeProgress;
        };

        // A collapsed navigation being built.
        struct NavigationLevel
        {
            ID Id;
            Rect Area;
            /// 0 shows the root, 1 the detail.
            float Progress;
            bool IsInRoot;
            std::string_view RootTitle;
            bool IsRootBackPressed;
        };

        struct NavigationBuild
        {
            uint64_t Frame;
            int Depth;
            NavigationLevel Levels[MaxDepth];
        };

        NavigationBuild& GetBuild()
        {
            NavigationBuild& build =
                *GetState<NavigationBuild>(HashID("Carbon.Navigation.Build"), StateLifetime::Persistent);
            if (build.Frame != GetFrameCount())
            {
                build.Frame = GetFrameCount();
                build.Depth = 0;
            }
            return build;
        }

        NavigationState& GetNavigationState(ID id)
        {
            return *GetState<NavigationState>(id, StateLifetime::Persistent);
        }

        ID GetProgressID(ID id)
        {
            return HashID("##navigation", id);
        }

        // The bar at the top of a pane. Returns true when its back button was pressed.
        bool NavigationBar(ID id, std::string_view title, std::string_view backTitle, bool hasBack)
        {
            ItemOptions item;
            item.Width = Size::Fill();
            const Rect bar = AllocateItem(Vec2(0.0f, BarHeight), item);
            DrawList& drawList = GetDrawList();
            const float pixel = GetContentScale().GetPixelSize();
            drawList.AddRect(Rect(bar.X, bar.GetBottom() - pixel, bar.Width, pixel),
                             GetStyleColor(StyleColor::Separator));

            // The title is centered; the back button's label gives way to it, as in UIKit: the previous title,
            // else "Back", else the chevron alone.
            TextSpec titleSpec = GetTextSpec(TextStyle::Headline);
            titleSpec.Wraps = false;
            const float titleWidth = title.empty() ? 0.0f : MeasureText(title, titleSpec).X;
            bool isBackPressed = false;
            float backWidth = 0.0f;
            if (hasBack)
            {
                const TextSpec spec = GetTextSpec(TextStyle::Body);
                const float room = (bar.Width - titleWidth) * 0.5f - BarPadding * 3.0f - BackIconSize - BackGap;
                std::string_view label = backTitle.empty() ? std::string_view("Back") : backTitle;
                if (MeasureText(label, spec).X > room)
                    label = MeasureText("Back", spec).X <= room ? std::string_view("Back") : std::string_view();
                const float labelWidth = label.empty() ? 0.0f : std::min(MeasureText(label, spec).X, bar.Width * 0.3f);
                backWidth = BackIconSize + BackGap + labelWidth + BarPadding;
                const Rect button(bar.X, bar.Y, backWidth + BarPadding, bar.Height);
                ButtonBehaviorOptions behavior;
                behavior.Focusable = true;
                const ID backID = HashID("##back", id);
                const Interaction interaction = ButtonBehavior(backID, button, behavior);
                isBackPressed = interaction.Clicked;
                // As UIKit's bar buttons: accent-colored, dimmed while pressed.
                const ControlFeedback feedback = AnimateFeedback(backID, interaction.Hovered, interaction.Pressed);
                const Color accent =
                    GetStyleColor(StyleColor::Accent).WithOpacity(1.0f - 0.6f * feedback.Press - 0.2f * feedback.Hover);
                const float centerY = bar.GetCenter().Y;
                DrawIcon(drawList, Vec2(bar.X + BarPadding + BackIconSize * 0.5f, centerY), Icons::CaretLeft,
                         BackIconSize, accent, IconVariant::Bold);
                TextSpec labelSpec = spec;
                labelSpec.MaxWidth = labelWidth;
                labelSpec.Wraps = false;
                DrawLabel(drawList, bar, bar.X + BarPadding + BackIconSize + BackGap, label, labelSpec, accent);
                DrawFocusRing(backID, button.Inset(EdgeInsets(2.0f, 6.0f)), 6.0f);
            }

            if (!title.empty())
            {
                titleSpec.MaxWidth = std::max(bar.Width - (backWidth + BarPadding) * 2.0f, 1.0f);
                const float width = std::min(titleWidth, titleSpec.MaxWidth);
                DrawLabel(drawList, bar, bar.GetCenter().X - width * 0.5f, title, titleSpec,
                          GetStyleColor(StyleColor::Label));
            }
            return isBackPressed;
        }
    } // namespace

    void BeginCollapsedNavigation(ID id, std::string_view rootTitle, Size width, Size height,
                                  std::string_view rootBackTitle)
    {
        NavigationBuild& build = GetBuild();
        const bool hasRoom = build.Depth < MaxDepth;
        CB_VERIFY(hasRoom, "Navigation stacks can be nested at most {} deep", MaxDepth);
        NavigationState& state = GetNavigationState(id);

        // The ID gives the stack its identity without changing the IDs of the panes' content.
        PushID(id);
        BeginVStack({.Spacing = 0.0f, .Width = width, .Height = height, .ID = "##navigation"});
        PopID();
        const Rect area = GetContentRect();

        // A swipe from the leading edge of the detail takes it back, following the finger.
        const ID progressID = GetProgressID(id);
        Pan swipe;
        if (state.IsDetailShown)
            swipe = PanBehavior(HashID("##swipeback", id), Rect(area.X, area.Y, EdgeWidth, area.Height),
                                {.Directions = PanDirections::Right, .Priority = 1});
        float progress = 0.0f;
        if (swipe.Active)
        {
            state.IsSwiping = true;
            progress = std::clamp(1.0f - swipe.Translation.X / std::max(area.Width, 1.0f), 0.0f, 1.0f);
            state.SwipeProgress = progress;
            SetAnimationValue(progressID, progress);
            RequestAnimationFrame();
        }
        else
        {
            if (state.IsSwiping)
            {
                // Released: back when past half the width or thrown; otherwise the detail returns.
                state.IsSwiping = false;
                if (state.SwipeProgress < 0.5f || (swipe.Ended && swipe.Velocity.X > PopVelocity))
                    state.IsDetailShown = false;
            }
            progress = std::clamp(Animate(progressID, state.IsDetailShown ? 1.0f : 0.0f, PushSpring), 0.0f, 1.0f);
        }

        GetDrawList().PushClipRect(area);
        if (hasRoom)
        {
            NavigationLevel& level = build.Levels[build.Depth];
            level.Id = id;
            level.Area = area;
            level.Progress = progress;
            level.IsInRoot = true;
            level.RootTitle = rootTitle;
            level.IsRootBackPressed = false;
        }
        build.Depth++;

        // The root: it fills the area and moves aside while the detail slides over it. Only the part the detail
        // leaves uncovered is drawn and takes the pointer.
        const float detailX = GetContentScale().Snap(area.X + (1.0f - progress) * area.Width);
        GetDrawList().PushClipRect(Rect(area.X, area.Y, std::max(detailX - area.X, 0.0f), area.Height));
        SetCursorPos(Vec2(area.X - progress * area.Width * Parallax, area.Y));
        BeginVStack({.Spacing = 0.0f, .Width = area.Width, .Height = area.Height});
        if (!rootTitle.empty() || !rootBackTitle.empty())
        {
            const bool isBackPressed =
                NavigationBar(HashID("##root", id), rootTitle, rootBackTitle, !rootBackTitle.empty());
            if (hasRoom)
                build.Levels[build.Depth - 1].IsRootBackPressed = isBackPressed;
        }
    }

    void CollapsedNavigationDetail(std::string_view detailTitle)
    {
        NavigationBuild& build = GetBuild();
        const bool isOpen = build.Depth > 0 && build.Depth <= MaxDepth;
        CB_VERIFY(isOpen, "CollapsedNavigationDetail without BeginCollapsedNavigation");
        if (!isOpen)
            return;
        NavigationLevel& level = build.Levels[build.Depth - 1];
        level.IsInRoot = false;
        EndVStack();
        GetDrawList().PopClipRect();

        const Rect& area = level.Area;
        const float progress = level.Progress;
        DrawList& drawList = GetDrawList();
        if (progress > 0.0f && progress < 1.0f)
            drawList.AddRect(area, Color::Black().WithOpacity(RootDimming * progress));

        // The detail: it slides in from the trailing edge over the root, with a shadow along its leading edge.
        const float x = GetContentScale().Snap(area.X + (1.0f - progress) * area.Width);
        if (progress > 0.0f && progress < 1.0f)
        {
            drawList.AddShadow(Rect(x, area.Y, area.Width, area.Height),
                               GetStyleColor(StyleColor::Shadow).WithOpacity(0.5f), 0.0f, EdgeShadowBlur);
        }
        SetCursorPos(Vec2(x, area.Y));
        BeginVStack({.Spacing = 0.0f,
                     .Width = area.Width,
                     .Height = area.Height,
                     .Background = GetStyleColor(StyleColor::Background),
                     .CornerRadius = 0.0f});
        if (NavigationBar(level.Id, detailTitle, level.RootTitle, true))
            GetNavigationState(level.Id).IsDetailShown = false;
    }

    bool EndCollapsedNavigation()
    {
        NavigationBuild& build = GetBuild();
        const bool isOpen = build.Depth > 0;
        CB_VERIFY(isOpen, "EndCollapsedNavigation without BeginCollapsedNavigation");
        if (!isOpen)
            return false;
        build.Depth--;
        const NavigationLevel level = build.Levels[std::min(build.Depth, MaxDepth - 1)];
        EndVStack();

        // While the panes slide, nothing in them takes the pointer.
        if (level.Progress > 0.0f && level.Progress < 1.0f)
        {
            ButtonBehaviorOptions behavior;
            behavior.Focusable = false;
            ButtonBehavior(HashID("##transition", level.Id), level.Area, behavior);
            RequestAnimationFrame();
        }
        GetDrawList().PopClipRect();
        EndVStack();
        return level.IsRootBackPressed;
    }

    bool IsInCollapsedNavigationRoot()
    {
        NavigationBuild& build = GetBuild();
        return build.Depth > 0 && build.Depth <= MaxDepth && build.Levels[build.Depth - 1].IsInRoot;
    }

    void NotifyNavigationChoice()
    {
        if (IsInCollapsedNavigationRoot())
        {
            NavigationBuild& build = GetBuild();
            GetNavigationState(build.Levels[build.Depth - 1].Id).IsDetailShown = true;
        }
    }

    void SetNavigationDetailShown(ID id, bool isShown, bool animated)
    {
        GetNavigationState(id).IsDetailShown = isShown;
        if (!animated)
            SetAnimationValue(GetProgressID(id), isShown ? 1.0f : 0.0f);
    }

    bool IsNavigationDetailShown(ID id)
    {
        return GetNavigationState(id).IsDetailShown;
    }
} // namespace Carbon::Internal
