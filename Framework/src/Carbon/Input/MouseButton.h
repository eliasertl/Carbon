#pragma once

#include <cstdint>

namespace Carbon
{
    /// Mouse buttons.
    enum class MouseButton : uint8_t
    {
        Left,
        Right,
        Middle,
        Back,
        Forward,

        Count
    };
} // namespace Carbon
