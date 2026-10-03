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
        NotAllowed
    };
} // namespace Carbon
