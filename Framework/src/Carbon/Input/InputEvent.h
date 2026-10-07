#pragma once

#include <cstddef>
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
        Focus,
        CompositionStart,
        CompositionUpdate,
        CompositionCommit,
        CompositionCancel
    };

    /// A clause of an input method's composition: a run of the pre-edit text that the input method converts as a
    /// unit, such as one word of a Japanese sentence. Offsets are bytes of the pre-edit text (UTF-8).
    struct CompositionClause
    {
        size_t Start = 0;
        size_t End = 0;
        /// The clause the user is converting now. It is drawn with a thicker underline.
        bool IsActive = false;
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
        /// CompositionUpdate and CompositionCommit: where the text, and the clauses of an update, are stored in
        /// the IO object's queue, and the caret as a byte offset into the text.
        uint32_t TextStart = 0;
        uint32_t TextLength = 0;
        uint32_t ClauseStart = 0;
        uint32_t ClauseCount = 0;
        uint32_t Caret = 0;
    };
} // namespace Carbon
