#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
    struct Pan;

    /// Options of a scroll view. All fields are optional.
    struct ScrollViewOptions
    {
        /// The direction the content scrolls and its items are laid out in.
        Carbon::Axis Axis = Carbon::Axis::Vertical;
        /// Size of the visible area. Fill by default; inside a container that fits its content, give the
        /// scrolling axis a fixed size.
        Size Width = Size::Fill();
        Size Height = Size::Fill();
        /// Distance between items; the theme's Spacing when not set.
        std::optional<float> Spacing = {};
        /// Space around the content, scrolling with it.
        EdgeInsets Padding = {};
        /// Where items sit across the scrolling axis (horizontally for a vertical scroll view).
        Carbon::Alignment Alignment = Carbon::Alignment::Leading;
        /// Whether the overlay scroll indicator is shown while scrolling.
        bool ShowsIndicator = true;
    };

    /// Starts a scrollable area that clips its content. Items inside are laid out like in a stack along the
    /// scrolling axis. The mouse wheel scrolls the innermost scroll view under the pointer; the offset is kept
    /// per ID, and the ID is pushed on the ID stack. Every Begin needs a matching End.
    void BeginScrollView(std::string_view id, const ScrollViewOptions& options = {});
    void EndScrollView();

    /// The scroll offset of a scroll view, in points, addressed by the ID it was begun with (resolved in the
    /// current ID scope).
    Vec2 GetScrollOffset(std::string_view id);
    /// Scrolls a scroll view to an offset; it is clamped to the content on the next frame. `animated` glides
    /// there instead of jumping.
    void SetScrollOffset(std::string_view id, Vec2 offset, bool animated = false);

    /// Scrolling one axis with a finger, as on iOS: the content follows the finger, resists when pulled beyond
    /// its ends, keeps the velocity it was thrown with and slows down, and springs back from beyond an end.
    /// ScrollView does this by itself; components that scroll on their own (Table scrolls its columns sideways)
    /// keep a TouchScroll in their per-ID state and drive it with the Pan of PanBehavior.
    struct TouchScroll
    {
        /// The offset to show while UpdateTouchScroll returns true; beyond 0 or the largest offset by a little
        /// while the content is pulled or bounces.
        float Offset = 0.0f;
        /// Points per second, in the direction the offset grows.
        float Velocity = 0.0f;
        /// The offset when the finger caught the content, without resistance.
        float StartOffset = 0.0f;
        /// A finger holds the content.
        bool IsTracking = false;
        /// The content slows down or springs back after the finger was lifted.
        bool IsMoving = false;
    };

    /// Advances a TouchScroll by a frame along `axis`. `currentOffset` is where the content is shown when a finger
    /// catches it, `maxOffset` the largest offset (content length minus viewport length) and `viewportLength`
    /// sets how strongly the content resists beyond its ends. Returns true when the touch scrolling decided the
    /// offset this frame: show `scroll.Offset` and keep the component's own target at it, clamped. While it
    /// returns true, call it every frame and keep frames coming.
    bool UpdateTouchScroll(TouchScroll& scroll, const Pan& pan, Axis axis, float currentOffset, float maxOffset,
                           float viewportLength);
} // namespace Carbon
