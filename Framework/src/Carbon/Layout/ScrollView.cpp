#include "Carbon/Layout/ScrollView.h"

#include <algorithm>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Layout/LayoutInternal.h"

namespace Carbon
{
    namespace
    {
        // Points scrolled per wheel notch: three lines of body text.
        constexpr float WheelStep = 48.0f;
        // Seconds the indicator stays visible after the last scroll.
        constexpr float IndicatorHoldTime = 1.0f;
        constexpr float IndicatorMargin = 2.0f;
        constexpr float IndicatorMinLength = 24.0f;

        // Survives while the scroll view is hidden, so returning to a view finds it where it was left.
        struct ScrollState
        {
            /// Where the view is scrolling to; the displayed offset follows with a spring.
            Vec2 Offset;
            /// Largest valid offset, from last frame's content and viewport sizes.
            Vec2 MaxOffset;
            /// Seconds since the offset last changed; the indicator fades after IndicatorHoldTime.
            float IdleTime;
            /// Set when the offset was assigned without animation: the displayed offset must jump too.
            bool IsJumpPending;
        };

        const AnimationSpec ScrollSpring = AnimationSpec::Spring(0.25f, 1.0f);

        ID GetOffsetAnimationID(ID scrollView)
        {
            return HashID("##offset", scrollView);
        }
    } // namespace

    void BeginScrollView(std::string_view id, const ScrollViewOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        const ID scrollID = GetID(id);
        ScrollState& state = *GetState<ScrollState>(scrollID, StateLifetime::Persistent);
        const bool isVertical = options.Axis == Axis::Vertical;

        // The wheel scrolls the innermost scroll view under the pointer, as determined during the last frame.
        if (context.Layout.HoveredScrollView == scrollID)
        {
            const Vec2 wheel = context.Input.MouseWheel;
            // A plain wheel has only a vertical axis; in a horizontal scroll view it scrolls sideways.
            const float amount = isVertical ? wheel.Y : (wheel.X != 0.0f ? wheel.X : wheel.Y);
            if (amount != 0.0f)
            {
                (isVertical ? state.Offset.Y : state.Offset.X) -= amount * WheelStep;
                state.IdleTime = 0.0f;
            }
        }
        state.Offset = Max(Vec2(), Min(state.Offset, state.MaxOffset));
        if (state.IsJumpPending)
        {
            SetAnimationValue(GetOffsetAnimationID(scrollID), state.Offset);
            state.IsJumpPending = false;
        }

        // The displayed offset glides to the target and sits on whole pixels so text stays crisp.
        const Vec2 displayed = context.Scale.Snap(Animate(GetOffsetAnimationID(scrollID), state.Offset, ScrollSpring));

        Internal::ContainerDescription description;
        description.Kind = Internal::ContainerKind::ScrollView;
        description.Axis = options.Axis;
        description.Id = scrollID;
        description.Width = options.Width;
        description.Height = options.Height;
        description.Padding = options.Padding;
        description.Spacing = options.Spacing.value_or(context.Style.Current.GetVar(StyleVar::Spacing));
        description.CrossFactor =
            options.Alignment == Alignment::Leading ? 0.0f : (options.Alignment == Alignment::Center ? 0.5f : 1.0f);
        description.IsScrolling = true;
        description.ScrollOffset = displayed;
        const Internal::LayoutFrame& frame = Internal::BeginContainer(context, description);

        const Rect viewport(frame.Origin, frame.ResolvedSize);
        context.Draw.PushClipRect(viewport);
        if (context.Input.HasMousePos && context.Draw.GetClipRect().Contains(context.Input.MousePos))
            context.Layout.HoveredScrollViewCandidate = scrollID;

        Internal::ScrollFrame scrollFrame;
        scrollFrame.Id = scrollID;
        scrollFrame.Viewport = viewport;
        scrollFrame.Axis = options.Axis;
        scrollFrame.ShowsIndicator = options.ShowsIndicator;
        scrollFrame.DisplayedOffset = displayed;
        context.Layout.ScrollFrames.push_back(scrollFrame);

        PushID(scrollID);
    }

    void EndScrollView()
    {
        Context& context = Internal::GetFrameContext();
        const bool isOpen = !context.Layout.ScrollFrames.empty() && context.Layout.Frames.size() > 1 &&
                            context.Layout.Frames.back().Kind == Internal::ContainerKind::ScrollView;
        CB_VERIFY(isOpen, "EndScrollView does not match the innermost open Begin call");
        if (!isOpen)
            return;

        PopID();
        const Internal::ScrollFrame scrollFrame = context.Layout.ScrollFrames.back();
        context.Layout.ScrollFrames.pop_back();
        const Internal::LayoutFrame& frame = context.Layout.Frames.back();
        ScrollState& state = *GetState<ScrollState>(scrollFrame.Id, StateLifetime::Persistent);
        const bool isVertical = scrollFrame.Axis == Axis::Vertical;

        // How far the content, with its padding, extends beyond the viewport.
        const float padding = isVertical ? frame.Padding.GetVertical() : frame.Padding.GetHorizontal();
        const float viewportLength = isVertical ? scrollFrame.Viewport.Height : scrollFrame.Viewport.Width;
        const float contentLength = frame.MainExtent + padding;
        const float maxOffset = std::max(0.0f, contentLength - viewportLength);
        state.MaxOffset = isVertical ? Vec2(0.0f, maxOffset) : Vec2(maxOffset, 0.0f);

        // The overlay indicator: visible while scrolling, fading out once the view has been idle.
        state.IdleTime = std::min(state.IdleTime + context.DeltaTime, IndicatorHoldTime * 2.0f);
        const bool isActive = maxOffset > 0.0f && state.IdleTime < IndicatorHoldTime;
        if (isActive)
            context.IsAnimatingThisFrame = true;
        const float opacity =
            Animate(HashID("##indicator", scrollFrame.Id), isActive ? 1.0f : 0.0f, AnimationSpec::Fade(0.25f));
        if (scrollFrame.ShowsIndicator && opacity > 0.0f && maxOffset > 0.0f)
        {
            const float thickness = context.Style.Current.GetVar(StyleVar::ScrollIndicatorWidth);
            const float trackLength = viewportLength - IndicatorMargin * 2.0f;
            const float thumbLength = std::clamp(trackLength * viewportLength / contentLength,
                                                 std::min(IndicatorMinLength, trackLength), trackLength);
            const float offset = isVertical ? scrollFrame.DisplayedOffset.Y : scrollFrame.DisplayedOffset.X;
            const float travel = (trackLength - thumbLength) * std::clamp(offset / maxOffset, 0.0f, 1.0f);

            const Rect& viewport = scrollFrame.Viewport;
            const Rect thumb = isVertical
                                   ? Rect(viewport.GetRight() - IndicatorMargin - thickness,
                                          viewport.Y + IndicatorMargin + travel, thickness, thumbLength)
                                   : Rect(viewport.X + IndicatorMargin + travel,
                                          viewport.GetBottom() - IndicatorMargin - thickness, thumbLength, thickness);
            const Color color = context.Style.Current.GetColor(StyleColor::ScrollIndicator).WithOpacity(opacity);
            context.Draw.AddSquircle(thumb, color, thickness * 0.5f, 0.0f);
        }

        context.Draw.PopClipRect();
        Internal::EndContainer(context, Internal::ContainerKind::ScrollView);
    }

    Vec2 GetScrollOffset(std::string_view id)
    {
        return GetState<ScrollState>(GetID(id), StateLifetime::Persistent)->Offset;
    }

    void SetScrollOffset(std::string_view id, Vec2 offset, bool animated)
    {
        const ID scrollID = GetID(id);
        ScrollState& state = *GetState<ScrollState>(scrollID, StateLifetime::Persistent);
        state.Offset = Max(Vec2(), offset);
        state.IdleTime = 0.0f;
        state.IsJumpPending = !animated;
    }
} // namespace Carbon
