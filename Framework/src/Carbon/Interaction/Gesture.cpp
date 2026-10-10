#include "Carbon/Interaction/Gesture.h"

#include <algorithm>
#include <cmath>

#include "Carbon/Animation/Animation.h"
#include "Carbon/Core/ContextInternal.h"
#include "Carbon/Core/State.h"
#include "Carbon/Interaction/GestureInternal.h"
#include "Carbon/Overlay/OverlayInternal.h"

namespace Carbon
{
    namespace
    {
        using Internal::GestureState;
        using Internal::InputState;

        constexpr size_t LeftButton = static_cast<size_t>(MouseButton::Left);

        // Beyond its limits a pinch keeps this much of its effect (as an exponent), so it resists more the
        // further it goes.
        constexpr float ZoomResistance = 0.25f;
        const AnimationSpec ZoomSpring = AnimationSpec::Spring(0.35f, 1.0f);

        // The zoom of one item. While two fingers drive it, the values follow them directly; afterwards the
        // displayed values glide to the target on a spring.
        struct ZoomState
        {
            float Scale;
            Vec2 Offset;
            float TargetScale;
            Vec2 TargetOffset;
            /// The values when the fingers came down.
            float StartScale;
            Vec2 StartOffset;
            bool IsInitialized;
        };

        // A pointer down by a finger, in a frame in which the item may react to it.
        bool IsTouchPointerDown(const InputState& input)
        {
            return input.IsPointerTouch && input.HasPrimaryTouch && input.MouseDown[LeftButton];
        }

        // True when `point` is inside the current clip rectangle and not covered by an overlay above the layer
        // being drawn: where a touch at `point` reaches what is being built.
        bool IsPointReachable(const Context& context, Vec2 point)
        {
            return context.Draw.GetClipRect().Contains(point) && !Internal::IsPointBlockedByOverlay(context, point);
        }

        PanDirections GetDirection(Vec2 travel)
        {
            if (std::abs(travel.X) >= std::abs(travel.Y))
                return travel.X < 0.0f ? PanDirections::Left : PanDirections::Right;
            return travel.Y < 0.0f ? PanDirections::Up : PanDirections::Down;
        }

        // A value beyond `limit`, on either side, keeps a fraction of how far it went: limit * (value / limit) ^
        // resistance.
        float Resist(float value, float limit)
        {
            if (limit <= 0.0f || value <= 0.0f)
                return limit;
            return limit * std::pow(value / limit, ZoomResistance);
        }

        // Offsets that keep zoomed content covering `size`: from size * (1 - scale) to 0, or centered when the
        // content is smaller.
        Vec2 ClampOffset(Vec2 offset, Vec2 size, float scale)
        {
            const auto clampAxis = [scale](float value, float length)
            {
                const float least = length * (1.0f - scale);
                if (least > 0.0f)
                    return least * 0.5f;
                return std::clamp(value, least, 0.0f);
            };
            return Vec2(clampAxis(offset.X, size.X), clampAxis(offset.Y, size.Y));
        }

        // The same with resistance beyond the bounds, while fingers hold the content.
        Vec2 ResistOffset(Vec2 offset, Vec2 size, float scale)
        {
            const Vec2 clamped = ClampOffset(offset, size, scale);
            const auto resistAxis = [](float value, float bound, float length)
            {
                const float over = value - bound;
                if (over == 0.0f || length <= 0.0f)
                    return value;
                const float magnitude = std::abs(over);
                const float resisted = (1.0f - 1.0f / (magnitude * 0.55f / length + 1.0f)) * length;
                return bound + (over < 0.0f ? -resisted : resisted);
            };
            return Vec2(resistAxis(offset.X, clamped.X, size.X), resistAxis(offset.Y, clamped.Y, size.Y));
        }

        const Internal::TouchPoint* FindTouch(const InputState& input, uint64_t id)
        {
            for (size_t i = 0; i < input.TouchCount; i++)
            {
                if (input.Touches[i].Id == id)
                    return &input.Touches[i];
            }
            return nullptr;
        }
    } // namespace

    namespace Internal
    {
        void BeginGestures(Context& context)
        {
            GestureState& state = context.Gestures;
            const InputState& input = context.Input;
            state.PanCandidate = ID();
            state.PanCandidatePriority = 0;
            state.ZoomCandidate = ID();
            state.IsPanAlive = false;
            state.IsZoomAlive = false;
            state.IsLongPressed = false;

            // The pan whose finger was lifted reports its end during this frame.
            state.IsPanEnding = state.PanOwner.IsValid() && !IsTouchPointerDown(input);

            if (!IsTouchPointerDown(input))
            {
                state.IsPastSlop = false;
                state.HasLongPressed = false;
                return;
            }

            const Vec2 travel = input.MousePos - input.MousePressedPos[LeftButton];
            if (travel.GetLengthSquared() > TouchSlop * TouchSlop)
                state.IsPastSlop = true;

            // A finger that rests on one spot long enough is a long press, unless it already does something else.
            const bool isFree = !state.IsPastSlop && !state.HasLongPressed && !state.PanOwner.IsValid() &&
                                !state.ZoomOwner.IsValid() && input.TouchCount <= 1;
            if (!isFree)
                return;
            const double held = context.Time - input.MouseLastPressTime[LeftButton];
            if (held >= LongPressDuration)
            {
                state.IsLongPressed = true;
                state.HasLongPressed = true;
                state.LongPressPosition = input.MousePos;
                context.IsAnimatingThisFrame = true;
            }
            else
            {
                RequestFrameAfter(static_cast<float>(LongPressDuration - held));
            }
        }

        void EndGestures(Context& context)
        {
            GestureState& state = context.Gestures;
            InteractionState& interaction = context.Interaction;

            // A pan ends with its finger, or when its component was not submitted.
            if (state.PanOwner.IsValid() && (state.IsPanEnding || !state.IsPanAlive))
            {
                if (interaction.ActiveID == state.PanOwner)
                    interaction.ActiveID = ID();
                state.PanOwner = ID();
            }
            state.IsPanBeginning = false;
            state.IsPanEnding = false;

            // A zoom ends when one of its fingers is lifted.
            if (state.ZoomOwner.IsValid())
            {
                const bool hasFingers = FindTouch(context.Input, state.ZoomTouches[0]) != nullptr &&
                                        FindTouch(context.Input, state.ZoomTouches[1]) != nullptr;
                if (!hasFingers || !state.IsZoomAlive)
                {
                    if (interaction.ActiveID == state.ZoomOwner)
                        interaction.ActiveID = ID();
                    state.ZoomOwner = ID();
                }
            }

            // Two fingers on zoomable content take precedence over a pan of one of them.
            if (state.ZoomCandidate.IsValid() && !state.ZoomOwner.IsValid())
            {
                const InputState& input = context.Input;
                const TouchPoint& first = input.Touches[0];
                const TouchPoint& second = input.Touches[1];
                state.ZoomOwner = state.ZoomCandidate;
                state.ZoomTouches[0] = first.Id;
                state.ZoomTouches[1] = second.Id;
                state.ZoomStartDistance = (second.Position - first.Position).GetLength();
                state.ZoomStartCenter = (first.Position + second.Position) * 0.5f;
                if (state.PanOwner.IsValid())
                    state.PanOwner = ID();
                interaction.ActiveID = state.ZoomOwner;
                interaction.IsActiveAlive = true;
                interaction.IsActiveDrag = true;
                return;
            }

            if (state.PanCandidate.IsValid() && !state.PanOwner.IsValid() && !state.ZoomOwner.IsValid())
            {
                state.PanOwner = state.PanCandidate;
                state.PanStart = context.Input.MousePos;
                state.IsPanBeginning = true;
                // The pan holds the pointer from now on: the control the finger started on is not activated.
                interaction.ActiveID = state.PanOwner;
                interaction.IsActiveAlive = true;
                interaction.IsActiveDrag = true;
            }
        }

        void CancelPointerPress(Context& context)
        {
            context.Interaction.ActiveID = ID();
            context.Interaction.IsActiveDrag = false;
        }
    } // namespace Internal

    Pan PanBehavior(ID id, const Rect& rect, const PanOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        GestureState& state = context.Gestures;
        Internal::InteractionState& interaction = context.Interaction;
        const InputState& input = context.Input;
        Pan result;
        if (!id.IsValid() || !input.IsPointerTouch)
            return result;

        if (state.PanOwner == id)
        {
            state.IsPanAlive = true;
            result.Position = input.MousePos;
            result.Translation = input.MousePos - state.PanStart;
            result.Velocity = input.PointerVelocity;
            if (state.IsPanEnding)
            {
                result.Ended = true;
                return result;
            }
            if (interaction.ActiveID == id)
                interaction.IsActiveAlive = true;
            result.Active = true;
            result.Began = state.IsPanBeginning;
            result.Delta = result.Began ? result.Translation : input.MouseDelta;
            return result;
        }

        // Claiming: a finger that started inside the rectangle and has just moved past the slop, in a direction
        // the pan accepts, unless something else already follows it.
        if (!IsTouchPointerDown(input) || !state.IsPastSlop || state.PanOwner.IsValid() || state.ZoomOwner.IsValid() ||
            interaction.DisabledDepth > 0)
            return result;
        if (interaction.ActiveID.IsValid() && interaction.IsActiveDrag)
            return result;
        const Vec2 start = input.MousePressedPos[LeftButton];
        if (!rect.Contains(start) || !IsPointReachable(context, start))
            return result;
        const PanDirections direction = GetDirection(input.MousePos - start);
        if ((options.Directions & direction) == PanDirections::None)
            return result;
        if (!state.PanCandidate.IsValid() || options.Priority >= state.PanCandidatePriority)
        {
            state.PanCandidate = id;
            state.PanCandidatePriority = options.Priority;
        }
        return result;
    }

    bool IsItemLongPressed()
    {
        Context& context = Internal::GetFrameContext();
        const GestureState& state = context.Gestures;
        if (!state.IsLongPressed)
            return false;
        const Rect& bounds = context.Interaction.LastItem.Bounds;
        return bounds.Contains(state.LongPressPosition) && IsPointReachable(context, state.LongPressPosition);
    }

    Zoom ZoomBehavior(ID id, const Rect& rect, const ZoomOptions& options)
    {
        Context& context = Internal::GetFrameContext();
        GestureState& gestures = context.Gestures;
        const InputState& input = context.Input;
        ZoomState& state = *GetState<ZoomState>(id);
        const ID scaleID = HashID("##zoomscale", id);
        const ID offsetID = HashID("##zoomoffset", id);
        const float minScale = std::max(options.MinScale, 0.01f);
        const float maxScale = std::max(options.MaxScale, minScale);
        if (!state.IsInitialized)
        {
            state.IsInitialized = true;
            state.Scale = std::clamp(1.0f, minScale, maxScale);
            state.TargetScale = state.Scale;
            state.Offset = ClampOffset(Vec2(), rect.GetSize(), state.Scale);
            state.TargetOffset = state.Offset;
            SetAnimationValue(scaleID, state.Scale);
            SetAnimationValue(offsetID, state.Offset);
        }

        Zoom result;
        if (gestures.ZoomOwner == id)
        {
            gestures.IsZoomAlive = true;
            if (context.Interaction.ActiveID == id)
                context.Interaction.IsActiveAlive = true;
            const Internal::TouchPoint* first = FindTouch(input, gestures.ZoomTouches[0]);
            const Internal::TouchPoint* second = FindTouch(input, gestures.ZoomTouches[1]);
            if (first != nullptr && second != nullptr)
            {
                if (state.StartScale == 0.0f)
                {
                    state.StartScale = state.Scale;
                    state.StartOffset = state.Offset;
                }
                const float distance = (second->Position - first->Position).GetLength();
                const Vec2 center = (first->Position + second->Position) * 0.5f;
                float scale = state.StartScale;
                if (gestures.ZoomStartDistance > 0.0f)
                    scale = state.StartScale * distance / gestures.ZoomStartDistance;
                if (scale > maxScale)
                    scale = Resist(scale, maxScale);
                else if (scale < minScale)
                    scale = Resist(scale, minScale);

                // The point of the content under the fingers when they came down stays under them.
                const Vec2 origin = rect.GetMin();
                const Vec2 anchor = (gestures.ZoomStartCenter - origin - state.StartOffset) / state.StartScale;
                const Vec2 offset = center - origin - anchor * scale;
                state.Scale = scale;
                state.Offset = ResistOffset(offset, rect.GetSize(), scale);
                state.TargetScale = std::clamp(scale, minScale, maxScale);
                state.TargetOffset =
                    ClampOffset(center - origin - anchor * state.TargetScale, rect.GetSize(), state.TargetScale);
                SetAnimationValue(scaleID, state.Scale);
                SetAnimationValue(offsetID, state.Offset);
                result.Scale = state.Scale;
                result.Offset = state.Offset;
                result.Active = true;
                context.IsAnimatingThisFrame = true;
                return result;
            }
        }
        state.StartScale = 0.0f;

        // Claiming: the first two fingers both came down on the content.
        if (!gestures.ZoomOwner.IsValid() && input.TouchCount >= 2 && context.Interaction.DisabledDepth == 0)
        {
            const Vec2 a = input.Touches[0].StartPosition;
            const Vec2 b = input.Touches[1].StartPosition;
            if (rect.Contains(a) && rect.Contains(b) && IsPointReachable(context, a) && IsPointReachable(context, b))
                gestures.ZoomCandidate = id;
        }

        // A double tap zooms in around the tap, or back out.
        if (options.DoubleTapScale > 0.0f && input.IsPointerTouch && input.MousePressed[LeftButton] &&
            input.MouseClickCount[LeftButton] == 2 && rect.Contains(input.MousePos) &&
            IsPointReachable(context, input.MousePos))
        {
            const Vec2 origin = rect.GetMin();
            if (state.TargetScale > minScale + 0.01f)
            {
                state.TargetScale = minScale;
                state.TargetOffset = ClampOffset(Vec2(), rect.GetSize(), minScale);
            }
            else
            {
                const float scale = std::clamp(options.DoubleTapScale, minScale, maxScale);
                const Vec2 anchor = (input.MousePos - origin - state.Offset) / state.Scale;
                state.TargetScale = scale;
                state.TargetOffset = ClampOffset(input.MousePos - origin - anchor * scale, rect.GetSize(), scale);
            }
        }

        // The targets follow the limits, which the caller may change.
        state.TargetScale = std::clamp(state.TargetScale, minScale, maxScale);
        state.TargetOffset = ClampOffset(state.TargetOffset, rect.GetSize(), state.TargetScale);
        state.Scale = Animate(scaleID, state.TargetScale, ZoomSpring);
        state.Offset = Animate(offsetID, state.TargetOffset, ZoomSpring);
        result.Scale = state.Scale;
        result.Offset = state.Offset;
        return result;
    }

    void SetZoom(ID id, float scale, Vec2 offset, bool animated)
    {
        ZoomState& state = *GetState<ZoomState>(id);
        state.IsInitialized = true;
        state.TargetScale = scale;
        state.TargetOffset = offset;
        if (!animated)
        {
            state.Scale = scale;
            state.Offset = offset;
            SetAnimationValue(HashID("##zoomscale", id), scale);
            SetAnimationValue(HashID("##zoomoffset", id), offset);
        }
    }
} // namespace Carbon
