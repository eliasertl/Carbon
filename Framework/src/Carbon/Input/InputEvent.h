#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

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
        CompositionCancel,
        FileDrag,
        FileDragLeave,
        FileDrop,
        Touch,
        TextReplace
    };

    /// What a touch event reports about one finger (or pen) on the display.
    enum class TouchPhase : uint8_t
    {
        /// The finger touched the display.
        Began,
        /// The finger moved.
        Moved,
        /// The finger was lifted.
        Ended,
        /// The system took the touch away (a system gesture, an incoming call): it ends without activating
        /// anything.
        Cancelled
    };

    /// The kind of device that produced pointer input. Touch mode (Carbon/Input/Adaptive.h) follows the kind of
    /// the most recent pointer input.
    enum class PointerType : uint8_t
    {
        Mouse,
        Touch,
        /// A stylus; Carbon treats it like a finger.
        Pen
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
        /// MousePos, FileDrag, FileDrop and Touch: the position in points. MouseWheel: the scroll amount in lines.
        Vec2 Value;
        MouseButton Button = MouseButton::Left;
        Key KeyCode = Key::None;
        /// MouseButton and Key: pressed (true) or released (false). Focus: gained (true) or lost (false).
        bool Down = false;
        char32_t Character = 0;
        /// CompositionUpdate and CompositionCommit: where the text, and the clauses of an update, are stored in
        /// the IO object's queue, and the caret as a byte offset into the text. FileDrag and FileDrop: where the
        /// paths are stored, one after the other with a zero byte after each, and ClauseCount is their number.
        uint32_t TextStart = 0;
        uint32_t TextLength = 0;
        uint32_t ClauseStart = 0;
        uint32_t ClauseCount = 0;
        uint32_t Caret = 0;
        /// Touch: the host's identifier of the finger, its phase and the kind of device. A touch that drives the
        /// pointer begins and ends in two steps on consecutive frames; Down marks that the first one was applied.
        uint64_t TouchId = 0;
        TouchPhase Phase = TouchPhase::Began;
        PointerType Pointer = PointerType::Mouse;
        /// TextReplace: the byte range of the edited text that TextStart and TextLength replace.
        uint32_t ReplaceStart = 0;
        uint32_t ReplaceEnd = 0;
    };

    /// The kind of on-screen keyboard a text control asks for (TextFieldOptions::Keyboard).
    enum class KeyboardType : uint8_t
    {
        Text,
        /// Digits, a decimal separator and a sign.
        Number,
        Email,
        URL,
        Search
    };

    /// The text control being edited, for a host whose on-screen keyboard edits a copy of the text: an Android
    /// input connection, a browser's input element. Autocorrection and word suggestions need the text around the
    /// caret. See IO::GetTextInputState and IO::AddTextReplaceEvent.
    struct TextInputState
    {
        /// The whole text (UTF-8), without any pre-edit text of an input method. Valid until the next NewFrame.
        std::string_view Text = {};
        /// The selection as byte offsets into Text; equal for a caret.
        size_t SelectionStart = 0;
        size_t SelectionEnd = 0;
        KeyboardType Keyboard = KeyboardType::Text;
        /// Return inserts a new line (TextArea).
        bool IsMultiLine = false;
        /// A password: no suggestions, no autocorrection, nothing remembered.
        bool IsSecure = false;
    };
} // namespace Carbon
