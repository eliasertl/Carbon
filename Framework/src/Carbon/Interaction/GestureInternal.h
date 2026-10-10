#pragma once

#include <cstdint>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    struct Context;
}

namespace Carbon::Internal
{
    /// The touch gestures of one context. Like hover, a gesture is claimed during a frame and owned from the next
    /// one on; an owner holds the pointer (InteractionState::ActiveID) under its own ID.
    struct GestureState
    {
        /// The finger that drives the pointer has moved more than TouchSlop since it touched the display.
        bool IsPastSlop = false;

        /// The pan that follows the finger, and whether it was submitted this frame.
        ID PanOwner;
        bool IsPanAlive = false;
        /// PanOwner was recognized during the previous frame; this is the first frame it follows the finger.
        bool IsPanBeginning = false;
        /// The finger that drove PanOwner was lifted this frame; the pan ends with the frame.
        bool IsPanEnding = false;
        /// Where the finger was when the pan was recognized.
        Vec2 PanStart;
        /// The pan that claims the finger during this frame.
        ID PanCandidate;
        int PanCandidatePriority = 0;

        /// A long press was recognized this frame, at this position; it is recognized once per touch.
        bool IsLongPressed = false;
        bool HasLongPressed = false;
        Vec2 LongPressPosition;

        /// The zoom that two fingers drive, and the one that claims them during this frame.
        ID ZoomOwner;
        bool IsZoomAlive = false;
        ID ZoomCandidate;
        /// The two fingers of the zoom and their distance and midpoint when it began.
        uint64_t ZoomTouches[2] = {0, 0};
        float ZoomStartDistance = 0.0f;
        Vec2 ZoomStartCenter;
    };

    /// Called by NewFrame after BeginInteraction: tracks the slop and recognizes long presses.
    void BeginGestures(Context& context);
    /// Called by EndFrame before EndInteraction: hands the finger to the pan or zoom that claimed it.
    void EndGestures(Context& context);

    /// Takes the pointer away from the item that holds it, without activating it: a long press that opened a menu
    /// or a tooltip cancels the tap that would otherwise follow.
    void CancelPointerPress(Context& context);
} // namespace Carbon::Internal
