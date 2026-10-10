#include "Carbon/Layout/ScrollView.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Animation/Spring.h"
#include "Carbon/Core/Assert.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Input/Input.h"
#include "Carbon/Interaction/Gesture.h"
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
        // During a drag, the pointer this close to an edge scrolls the view, up to this fast in points per second.
        constexpr float AutoScrollZone = 32.0f;
        constexpr float AutoScrollSpeed = 900.0f;

        // Touch scrolling, after UIScrollView: thrown content keeps this fraction of its velocity per millisecond
        // (UIScrollView's normal deceleration rate) and stops below a few points per second; pulled beyond an end
        // it resists with UIScrollView's rubber-band constant, and it springs back on a critically damped spring.
        constexpr float DecelerationRate = 0.998f;
        constexpr float StopVelocity = 8.0f;
        constexpr float RubberBandConstant = 0.55f;
        constexpr float BounceResponse = 0.4f;
        // A finger that lands on content moving faster than this only stops it; it does not tap what is there.
        constexpr float CatchVelocity = 60.0f;

        // How far content pulled `distance` beyond an end is shown beyond it: it follows less the further it goes,
        // and never more than `length`.
        float RubberBand(float distance, float length)
        {
            return (1.0f - 1.0f / (distance * RubberBandConstant / length + 1.0f)) * length;
        }

        // The pull that shows content `shown` beyond an end; the inverse of RubberBand.
        float UnrubberBand(float shown, float length)
        {
            const float clamped = std::min(shown, length * 0.999f);
            return length * clamped / (RubberBandConstant * (length - clamped));
        }

        // An offset with resistance beyond 0 and `maxOffset`.
        float ResistOffset(float offset, float maxOffset, float length)
        {
            if (offset < 0.0f)
                return -RubberBand(-offset, length);
            if (offset > maxOffset)
                return maxOffset + RubberBand(offset - maxOffset, length);
            return offset;
        }

        // The offset without resistance that ResistOffset shows as `shown`.
        float UnresistOffset(float shown, float maxOffset, float length)
        {
            if (shown < 0.0f)
                return -UnrubberBand(-shown, length);
            if (shown > maxOffset)
                return maxOffset + UnrubberBand(shown - maxOffset, length);
            return shown;
        }

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
            /// Scrolling with a finger, the viewport a finger starts in (from last frame), and the offset shown last.
            TouchScroll Touch;
            Rect Viewport;
            Vec2 Displayed;
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
                state.Touch.IsMoving = false;
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

        // A finger drags the content along the scrolling axis, when there is something to scroll. A finger that
        // lands on content that is still moving stops it, and only that.
        const float maxOffset = isVertical ? state.MaxOffset.Y : state.MaxOffset.X;
        const Internal::InputState& input = context.Input;
        Internal::InteractionState& interaction = context.Interaction;
        const size_t leftButton = static_cast<size_t>(MouseButton::Left);
        const ID catchID = HashID("##catch", scrollID);
        if (state.Touch.IsMoving && input.IsPointerTouch && input.MousePressed[leftButton] &&
            state.Viewport.Contains(input.MousePos) && !Internal::IsPointerBlockedByOverlay(context))
        {
            if (std::abs(state.Touch.Velocity) > CatchVelocity && !interaction.ActiveID.IsValid())
            {
                interaction.ActiveID = catchID;
                interaction.IsActiveDrag = false;
            }
            state.Touch.IsMoving = false;
            state.Touch.Velocity = 0.0f;
        }
        if (interaction.ActiveID == catchID)
        {
            if (input.MouseDown[leftButton])
                interaction.IsActiveAlive = true;
            else
                interaction.ActiveID = ID();
        }
        PanOptions panOptions;
        panOptions.Directions = maxOffset <= 0.0f ? PanDirections::None
                                                  : (isVertical ? PanDirections::Vertical : PanDirections::Horizontal);
        const Pan pan = PanBehavior(HashID("##pan", scrollID), state.Viewport, panOptions);
        const float shown = isVertical ? state.Displayed.Y : state.Displayed.X;
        const bool isTouchScrolling =
            UpdateTouchScroll(state.Touch, pan, options.Axis, shown, maxOffset, state.ViewportLength);
        if (isTouchScrolling)
        {
            (isVertical ? state.Offset.Y : state.Offset.X) = std::clamp(state.Touch.Offset, 0.0f, maxOffset);
            state.IdleTime = 0.0f;
            context.IsAnimatingThisFrame = true;
        }

        state.Offset = Max(Vec2(), Min(state.Offset, state.MaxOffset));
        if (state.IsJumpPending)
        {
            SetAnimationValue(GetOffsetAnimationID(scrollID), state.Offset);
            state.IsJumpPending = false;
        }

        // The displayed offset glides to the target and sits on whole pixels so text stays crisp. A finger moves
        // it directly, also beyond the ends.
        Vec2 displayed;
        if (isTouchScrolling)
        {
            Vec2 touched = state.Offset;
            (isVertical ? touched.Y : touched.X) = state.Touch.Offset;
            SetAnimationValue(GetOffsetAnimationID(scrollID), touched);
            displayed = context.Scale.Snap(touched);
        }
        else
        {
            displayed = context.Scale.Snap(Animate(GetOffsetAnimationID(scrollID), state.Offset, ScrollSpring));
        }
        state.Displayed = displayed;

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
        state.Viewport = viewport;
        // Something dragged near an edge of the view under the pointer scrolls it, the faster the closer it gets.
        if (Internal::IsDragActive(context) && context.Layout.HoveredScrollView == scrollID &&
            context.Input.HasMousePos)
        {
            const float pointer = isVertical ? context.Input.MousePos.Y : context.Input.MousePos.X;
            const float start = isVertical ? viewport.Y : viewport.X;
            const float length = isVertical ? viewport.Height : viewport.Width;
            const float zone = std::min(AutoScrollZone, length * 0.25f);
            float depth = 0.0f;
            if (zone > 0.0f && pointer < start + zone)
                depth = -std::min((start + zone - pointer) / zone, 1.0f);
            else if (zone > 0.0f && pointer > start + length - zone)
                depth = std::min((pointer - (start + length - zone)) / zone, 1.0f);
            float& offset = isVertical ? state.Offset.Y : state.Offset.X;
            const float limit = isVertical ? state.MaxOffset.Y : state.MaxOffset.X;
            if ((depth < 0.0f && offset > 0.0f) || (depth > 0.0f && offset < limit))
            {
                offset = std::clamp(offset + depth * AutoScrollSpeed * context.DeltaTime, 0.0f, limit);
                state.IdleTime = 0.0f;
                context.IsAnimatingThisFrame = true;
            }
        }
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
        // A finger scrolls the content itself; the indicator only shows where it is, as on iOS.
        const bool isTouch = context.Input.IsPointerTouch;
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
            dragOptions.Disabled = isTouch;
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

            isLaneHovered = drag.Active || (!isTouch && IsRectHovered(lane) && !context.Interaction.ActiveID.IsValid());
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

    bool UpdateTouchScroll(TouchScroll& scroll, const Pan& pan, Axis axis, float currentOffset, float maxOffset,
                           float viewportLength)
    {
        const bool isVertical = axis == Axis::Vertical;
        const float length = std::max(viewportLength, 1.0f);
        maxOffset = std::max(maxOffset, 0.0f);
        // The offset grows when the finger moves towards the start: content follows the finger.
        const float velocity = -(isVertical ? pan.Velocity.Y : pan.Velocity.X);

        if (pan.Active)
        {
            if (pan.Began || !scroll.IsTracking)
            {
                // Caught where it is shown, also beyond an end, without a jump.
                scroll.StartOffset = UnresistOffset(currentOffset, maxOffset, length);
                scroll.IsTracking = true;
                scroll.IsMoving = false;
            }
            const float translation = isVertical ? pan.Translation.Y : pan.Translation.X;
            scroll.Offset = ResistOffset(scroll.StartOffset - translation, maxOffset, length);
            scroll.Velocity = velocity;
            return true;
        }
        if (scroll.IsTracking)
        {
            // Thrown: the content keeps the finger's velocity. A pan that vanished without a lift throws nothing.
            scroll.IsTracking = false;
            scroll.IsMoving = true;
            scroll.Velocity = pan.Ended ? velocity : 0.0f;
        }
        if (!scroll.IsMoving)
            return false;

        const float deltaTime = Internal::GetContext().DeltaTime;
        if (scroll.Offset >= 0.0f && scroll.Offset <= maxOffset)
        {
            scroll.Velocity *= std::pow(DecelerationRate, deltaTime * 1000.0f);
            scroll.Offset += scroll.Velocity * deltaTime;
            if (std::abs(scroll.Velocity) < StopVelocity)
            {
                scroll.Offset = std::clamp(scroll.Offset, 0.0f, maxOffset);
                scroll.Velocity = 0.0f;
                scroll.IsMoving = false;
            }
            return true;
        }

        // Beyond an end: a spring pulls the content back, after carrying it on a little with its velocity.
        const float end = scroll.Offset < 0.0f ? 0.0f : maxOffset;
        SpringState spring;
        spring.Value = scroll.Offset;
        spring.Velocity = scroll.Velocity;
        spring = AdvanceSpring(spring, end, BounceResponse, 1.0f, deltaTime);
        scroll.Offset = spring.Value;
        scroll.Velocity = spring.Velocity;
        if (IsSpringAtRest(spring, end))
        {
            scroll.Offset = end;
            scroll.Velocity = 0.0f;
            scroll.IsMoving = false;
        }
        return true;
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
