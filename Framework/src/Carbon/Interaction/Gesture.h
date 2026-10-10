#pragma once

#include <cstdint>

#include "Carbon/Core/ID.h"
#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"

namespace Carbon
{
    /// Touch gestures. A finger drives the pointer, so taps already work through ButtonBehavior; the functions here
    /// recognize what a mouse has no equivalent for: dragging content with a finger (pans), holding a finger still
    /// (long presses) and pinching with two fingers. They react only to touch and pen input: with a mouse, nothing
    /// in this header ever fires. See Docs/Mobile.md.

    /// How far a finger may move before its touch counts as a pan rather than a tap or a long press, in points.
    inline constexpr float TouchSlop = 10.0f;
    /// How long a finger has to rest for a long press, in seconds.
    inline constexpr float LongPressDuration = 0.5f;

    /// The directions a pan may begin in, as flags.
    enum class PanDirections : uint8_t
    {
        None = 0,
        Left = 1 << 0,
        Right = 1 << 1,
        Up = 1 << 2,
        Down = 1 << 3,
        Horizontal = Left | Right,
        Vertical = Up | Down,
        All = Horizontal | Vertical
    };

    constexpr PanDirections operator|(PanDirections a, PanDirections b)
    {
        return static_cast<PanDirections>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    constexpr PanDirections operator&(PanDirections a, PanDirections b)
    {
        return static_cast<PanDirections>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    /// Per-call options of PanBehavior. All fields are optional.
    struct PanOptions
    {
        /// The directions that begin the pan: the main direction of the finger's travel when it passes TouchSlop
        /// must be one of them. Once begun, the pan follows the finger in every direction.
        PanDirections Directions = PanDirections::All;
        /// When several pans could take the same finger, the highest priority wins, then the one submitted last
        /// (the innermost of nested scroll views). Navigation's swipe from the edge has priority 1 over content.
        int Priority = 0;
    };

    /// What a pan did this frame.
    struct Pan
    {
        /// The pan follows the finger: from the frame after it was recognized until the finger is lifted.
        bool Active = false;
        /// The first frame of Active.
        bool Began = false;
        /// The finger was lifted this frame (Active is false again). Velocity is what the pan was thrown with.
        bool Ended = false;
        /// Where the finger is, and how far it moved since the pan began (not since it touched the display: the
        /// slop is left out, so content that follows the translation does not jump).
        Vec2 Position;
        Vec2 Translation;
        /// Movement since the previous frame.
        Vec2 Delta;
        /// Points per second, smoothed.
        Vec2 Velocity;
    };

    /// Lets a finger drag `rect`'s content: scroll views, sheets that are pulled down, the swipe back of a
    /// navigation stack. A touch that starts inside `rect` (where it is visible) and moves more than TouchSlop in
    /// one of the allowed directions is recognized and, from the next frame on, holds the pointer under `id`:
    /// a control it started on is not activated. Controls that drag on their own (DragBehavior: sliders, dividers)
    /// keep their touch. Call it every frame, like ButtonBehavior.
    Pan PanBehavior(ID id, const Rect& rect, const PanOptions& options = {});

    /// True during the frame in which a finger that rests on the item submitted last, without moving more than
    /// TouchSlop, is recognized as a long press (after LongPressDuration). Context menus open and drags begin on
    /// it, where a mouse uses the right button or movement; tooltips show.
    bool IsItemLongPressed();

    /// Per-call options of ZoomBehavior. All fields are optional.
    struct ZoomOptions
    {
        /// The scale stays between these when the fingers are lifted; while pinching it may go a little beyond,
        /// with resistance, and springs back.
        float MinScale = 1.0f;
        float MaxScale = 4.0f;
        /// A double tap zooms to this scale around the tap, and back to MinScale when zoomed in. 0 turns it off.
        float DoubleTapScale = 2.0f;
    };

    /// The zoom of content. Content drawn at `rect` without zoom is drawn at GetZoomedRect(rect) instead,
    /// clipped to `rect`.
    struct Zoom
    {
        /// 1 is the content's own size.
        float Scale = 1.0f;
        /// Where the content's top-left corner moved, relative to the top-left of `rect`.
        Vec2 Offset;
        /// Two fingers are pinching or panning it.
        bool Active = false;

        /// The rectangle content of `rect` is drawn at.
        constexpr Rect GetZoomedRect(const Rect& rect) const
        {
            return Rect(rect.X + Offset.X, rect.Y + Offset.Y, rect.Width * Scale, rect.Height * Scale);
        }
    };

    /// Makes `rect`'s content zoomable with two fingers: pinching scales it around the point between the fingers,
    /// moving both fingers pans it while it is zoomed in, and a double tap zooms in and out. Beyond the limits it
    /// resists and springs back when the fingers are lifted. The zoom is kept per `id` while the item is shown.
    /// Mouse input does not zoom. Carbon never zooms the whole interface; this is for images, maps, documents.
    Zoom ZoomBehavior(ID id, const Rect& rect, const ZoomOptions& options = {});
    /// Sets the zoom of `id`, without animation when `animated` is false (for example to reset it when the content
    /// changes).
    void SetZoom(ID id, float scale, Vec2 offset = {}, bool animated = true);
} // namespace Carbon
