#pragma once

#include <optional>
#include <string_view>

#include "Carbon/Core/EdgeInsets.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
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
} // namespace Carbon
