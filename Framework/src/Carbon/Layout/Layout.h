#pragma once

#include "Carbon/Core/Rect.h"
#include "Carbon/Core/Vec2.h"
#include "Carbon/Layout/Size.h"

namespace Carbon
{
    /// How an item is sized inside its container.
    struct ItemOptions
    {
        /// Fit uses the size passed to AllocateItem; Fixed overrides it; Fill takes a share of the free space
        /// along the container's axis, or the container's full extent across it.
        Size Width = Size::Fit();
        Size Height = Size::Fit();
    };

    /// Reserves space for one item in the current container and returns its rectangle. This is how every widget
    /// gets its place: the item goes where the layout cursor is, and the cursor advances along the container's
    /// axis by the item's size plus the container's spacing.
    Rect AllocateItem(Vec2 size, const ItemOptions& options = {});

    /// Where the next item will be placed (its top-left corner, before alignment across the axis).
    Vec2 GetCursorPos();
    /// Places the next item's top-left corner at an absolute position instead of the flow position. The escape
    /// hatch for manual layout; items after it continue from where it ends.
    void SetCursorPos(Vec2 position);

    /// The content area of the current container: its rectangle minus padding. Uses last frame's measurement for
    /// axes that fit their content.
    Rect GetContentRect();
    /// The rectangle of the item allocated last in the current container.
    Rect GetLastItemRect();
} // namespace Carbon
