#pragma once

#include <cstdint>

namespace Carbon
{
    /// Mouse cursor shapes Carbon asks the host to show through the SetCursor callback.
    enum class Cursor : uint8_t
    {
        Arrow,
        IBeam,
        PointingHand,
        ResizeHorizontal,
        ResizeVertical,
        NotAllowed,
        /// Diagonal resizing, as at a window's corners: top-left to bottom-right, and top-right to bottom-left.
        ResizeTopLeftBottomRight,
        ResizeTopRightBottomLeft
    };
} // namespace Carbon
