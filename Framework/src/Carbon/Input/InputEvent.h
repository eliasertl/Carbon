#pragma once

#include <cstdint>

#include "Carbon/Core/Vec2.h"
#include "Carbon/Input/Key.h"
#include "Carbon/Input/MouseButton.h"

namespace Carbon
{
    /// Kinds of input the host can queue on the IO object.
    enum class InputEventType : uint8_t
    {
        MousePos,
        MouseLeave,
        MouseButton,
        MouseWheel,
        Key,
        Character,
        Focus
    };

    /// One queued input event. Which fields are meaningful depends on Type.
    struct InputEvent
    {
        InputEventType Type = InputEventType::MousePos;
        /// MousePos: the position in points. MouseWheel: the scroll amount in lines.
        Vec2 Value;
        MouseButton Button = MouseButton::Left;
        Key KeyCode = Key::None;
        /// MouseButton and Key: pressed (true) or released (false). Focus: gained (true) or lost (false).
        bool Down = false;
        char32_t Character = 0;
    };
} // namespace Carbon
