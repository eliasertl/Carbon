#include "Carbon/Layout/ScrollView.h"

#include <algorithm>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Interaction/Interaction.h"
#include "Carbon/Layout/LayoutInternal.h"
#include "Carbon/Overlay/OverlayInternal.h"

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
        // How much wider the indicator gets while the pointer is over its lane.
        constexpr float IndicatorHoverGrowth = 4.0f;

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
            /// Length of the visible area along the scrolling axis, from last frame.
            float ViewportLength;
            /// The offset at the moment a drag of the indicator started.
            float DragStartOffset;
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
        // Keyboard: Page Up and Page Down scroll the view under the pointer (or the outermost view when the
        // pointer is elsewhere), unless a text field is being edited. Home and End do the same when no control
        // has focus that might want those keys.
        if (context.Layout.KeyboardScrollView == scrollID && !context.TextEdit.Owner.IsValid() &&
            Internal::IsInActiveFocusScope(context))
        {
            float& offset = isVertical ? state.Offset.Y : state.Offset.X;
            const float before = offset;
            const float page = state.ViewportLength * 0.9f;
            if (IsKeyPressed(Key::PageDown))
                offset += page;
            if (IsKeyPressed(Key::PageUp))
                offset -= page;
            if (!context.Interaction.FocusedID.IsValid())
            {
                if (IsKeyPressed(Key::Home, false))
                    offset = 0.0f;
                if (IsKeyPressed(Key::End, false))
                    offset = isVertical ? state.MaxOffset.Y : state.MaxOffset.X;
            }
            if (offset != before)
                state.IdleTime = 0.0f;
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
        description.Spacing = options.Spacing.value_or(context.Style.GetVar(StyleVar::Spacing));
        description.CrossFactor =
            options.Alignment == Alignment::Leading ? 0.0f : (options.Alignment == Alignment::Center ? 0.5f : 1.0f);
        description.IsScrolling = true;
        description.ScrollOffset = displayed;
        const Internal::LayoutFrame& frame = Internal::BeginContainer(context, description);

        const Rect viewport(frame.Origin, frame.ResolvedSize);
        context.Draw.PushClipRect(viewport);
        // A scroll view covered by an overlay gets neither the wheel nor the page keys.
        if (IsRectHovered(viewport))
            context.Layout.HoveredScrollViewCandidate = scrollID;
        if (!context.Layout.FirstScrollViewCandidate.IsValid() && Internal::IsInActiveFocusScope(context))
            context.Layout.FirstScrollViewCandidate = scrollID;

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

        state.ViewportLength = viewportLength;

        // The overlay indicator: a pill at the trailing edge that appears while scrolling or while the pointer
        // is over its lane, can be dragged, and fades out once the view has been idle.
        const float thickness = context.Style.GetVar(StyleVar::ScrollIndicatorWidth);
        const float trackLength = viewportLength - IndicatorMargin * 2.0f;
        const bool canScroll = maxOffset > 0.0f && trackLength > 0.0f;
        const Rect& viewport = scrollFrame.Viewport;
        state.IdleTime = std::min(state.IdleTime + context.DeltaTime, IndicatorHoldTime * 2.0f);

        float thumbLength = 0.0f;
        float thumbRange = 0.0f;
        bool isLaneHovered = false;
        if (canScroll && scrollFrame.ShowsIndicator)
        {
            thumbLength = std::clamp(trackLength * viewportLength / contentLength,
                                     std::min(IndicatorMinLength, trackLength), trackLength);
            thumbRange = trackLength - thumbLength;
            const float offset = isVertical ? scrollFrame.DisplayedOffset.Y : scrollFrame.DisplayedOffset.X;
            const float travel = thumbRange * std::clamp(offset / maxOffset, 0.0f, 1.0f);

            // The lane is wider than the thumb so it is easy to hit.
            const float laneWidth = thickness + IndicatorMargin * 2.0f + IndicatorHoverGrowth;
            const Rect lane = isVertical
                                  ? Rect(viewport.GetRight() - laneWidth, viewport.Y, laneWidth, viewport.Height)
                                  : Rect(viewport.X, viewport.GetBottom() - laneWidth, viewport.Width, laneWidth);
            const Rect grip = isVertical ? Rect(lane.X, viewport.Y + IndicatorMargin + travel, laneWidth, thumbLength)
                                         : Rect(viewport.X + IndicatorMargin + travel, lane.Y, thumbLength, laneWidth);

            // The thumb is submitted after the content, so it wins the pointer over whatever is under it. It
            // must not become "the last item" for code that follows the scroll view.
            const Internal::InteractionState::LastItemData lastItem = context.Interaction.LastItem;
            DragBehaviorOptions dragOptions;
            dragOptions.Focusable = false;
            const DragInteraction drag = DragBehavior(HashID("##thumb", scrollFrame.Id), grip, dragOptions);
            context.Interaction.LastItem = lastItem;

            float& target = isVertical ? state.Offset.Y : state.Offset.X;
            if (drag.Started)
                state.DragStartOffset = target;
            if (drag.Active && thumbRange > 0.0f)
            {
                const float moved = isVertical ? drag.Total.Y : drag.Total.X;
                target = std::clamp(state.DragStartOffset + moved * maxOffset / thumbRange, 0.0f, maxOffset);
                state.IsJumpPending = true; // the content follows the thumb directly
            }

            isLaneHovered = drag.Active || (IsRectHovered(lane) && !context.Interaction.ActiveID.IsValid());
            if (isLaneHovered)
                state.IdleTime = 0.0f;
        }

        // The indicator stays for a moment after the last scroll. Nothing moves meanwhile: the next frame is due
        // when it starts to fade.
        const bool isActive = canScroll && state.IdleTime < IndicatorHoldTime;
        if (isActive)
            RequestFrameAfter(IndicatorHoldTime - state.IdleTime);
        const float opacity =
            Animate(HashID("##indicator", scrollFrame.Id), isActive ? 1.0f : 0.0f, AnimationSpec::Fade(0.25f));
        if (canScroll && scrollFrame.ShowsIndicator && opacity > 0.0f)
        {
            // Under the pointer the thumb grows a little, as macOS scrollers do.
            const float growth = Animate(HashID("##indicatorwidth", scrollFrame.Id),
                                         isLaneHovered ? IndicatorHoverGrowth : 0.0f, AnimationSpec::Spring(0.2f));
            const float width = thickness + growth;
            const float offset = isVertical ? scrollFrame.DisplayedOffset.Y : scrollFrame.DisplayedOffset.X;
            const float travel = thumbRange * std::clamp(offset / maxOffset, 0.0f, 1.0f);
            const Rect thumb = isVertical ? Rect(viewport.GetRight() - IndicatorMargin - width,
                                                 viewport.Y + IndicatorMargin + travel, width, thumbLength)
                                          : Rect(viewport.X + IndicatorMargin + travel,
                                                 viewport.GetBottom() - IndicatorMargin - width, thumbLength, width);
            const Color color = context.Style.GetColor(StyleColor::ScrollIndicator).WithOpacity(opacity);
            context.Draw.AddSquircle(thumb, color, width * 0.5f, 0.0f);
        }

        context.Draw.PopClipRect();
        Internal::EndContainer(context, Internal::ContainerKind::ScrollView);
    }

    void Internal::RevealInScrollViews(Context& context, const Rect& rect)
    {
        // Innermost first. Each view scrolls just far enough, with a little margin so the focus ring fits.
        const float margin = 6.0f;
        Rect target = rect.Expand(margin);
        for (size_t i = context.Layout.ScrollFrames.size(); i > 0; i--)
        {
            const Internal::ScrollFrame& scrollFrame = context.Layout.ScrollFrames[i - 1];
            ScrollState& state = *GetState<ScrollState>(scrollFrame.Id, StateLifetime::Persistent);
            const Rect& viewport = scrollFrame.Viewport;
            const bool isVertical = scrollFrame.Axis == Axis::Vertical;

            // The item is drawn at the displayed offset, which may still be gliding towards the target offset.
            // Decide from where the item will be once the view has arrived there.
            const Vec2 pending = state.Offset - scrollFrame.DisplayedOffset;
            target = target.Offset(-pending);

            const float start = isVertical ? target.Y - viewport.Y : target.X - viewport.X;
            const float end =
                isVertical ? target.GetBottom() - viewport.GetBottom() : target.GetRight() - viewport.GetRight();

            float delta = 0.0f;
            if (start < 0.0f)
                delta = start;
            else if (end > 0.0f)
                delta = std::min(end, start);
            if (delta == 0.0f)
                continue;

            (isVertical ? state.Offset.Y : state.Offset.X) += delta;
            state.IdleTime = 0.0f;
            // The outer views see the item where it will be once this view has scrolled.
            target = isVertical ? target.Offset(Vec2(0.0f, -delta)) : target.Offset(Vec2(-delta, 0.0f));
        }
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
