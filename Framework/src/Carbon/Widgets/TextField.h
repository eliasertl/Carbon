#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include "Carbon/Core/FunctionRef.h"
#include "Carbon/Layout/Size.h"
#include "Carbon/Widgets/ControlSize.h"

namespace Carbon
{
    /// Options of TextField. All fields are optional.
    struct TextFieldOptions
    {
        /// Shown in a faint color while the field is empty. Defaults to the field's label.
        std::string_view Placeholder = {};
        /// An icon from Carbon::Icons shown at the leading edge, e.g. a magnifying glass for a search field.
        std::string_view Icon = {};
        /// Shows a button at the trailing edge that clears the text while the field is not empty.
        bool ShowsClearButton = false;
        /// Width of the field. Text fields do not fit their content, so the default is a fixed 180 points.
        Size Width = Size::Fixed(180.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
        /// Displays bullets instead of the text, and keeps the text off the clipboard.
        bool IsSecure = false;
        /// Longest text the user can enter, in characters; 0 means unlimited.
        size_t MaxLength = 0;
        /// Points kept free at the trailing edge, for an accessory the caller draws there (the button of a combo
        /// box). Text and the clear button stay out of it.
        float TrailingInset = 0.0f;
        /// The up and down arrow keys move the caret to the start and end, as on macOS. Components that use those
        /// keys themselves (a combo box choosing from its list) turn this off.
        bool VerticalArrowsMoveCaret = true;
        /// Draws the field's background, border and focus ring. Components that draw their own field around the
        /// text (a token field) turn this off.
        bool IsBezeled = true;
        /// While false, the field keeps the focus but ignores the keyboard and the mouse and hides its caret and
        /// selection. Components that take the keys themselves for a while (a token field while tokens are
        /// selected) turn this off for those frames.
        bool AcceptsInput = true;
    };

    /// Where the caret and the selection of a text field are, as byte offsets into its text.
    struct TextFieldSelection
    {
        size_t Caret = 0;
        /// The selected range; Start == End when nothing is selected.
        size_t Start = 0;
        size_t End = 0;
    };

    /// A single-line text field editing `text` (UTF-8). Returns true on frames the text changed.
    ///
    /// Supports a caret, selection by mouse and keyboard, double click to select a word and triple click to
    /// select all, the clipboard (through the host's callbacks), and undo/redo. Enter leaves the text as it is
    /// and makes IsItemSubmitted() return true for this frame; Escape gives up focus.
    ///
    /// The label identifies the field and serves as its placeholder; put a Text next to the field for a
    /// visible title.
    bool TextField(std::string_view label, std::string* text, const TextFieldOptions& options = {});

    /// A text field editing a zero-terminated UTF-8 string in a fixed buffer owned by the caller, such as a
    /// `char name[64]` (arrays convert to the span by themselves). The text can take up all but the last byte,
    /// which holds the terminating zero. Typing or pasting more is cut off at the last whole character that fits,
    /// and nothing is written outside the buffer. Carbon never allocates for the buffer.
    bool TextField(std::string_view label, std::span<char> buffer, const TextFieldOptions& options = {});

    /// A text field for text the caller stores in its own way: `text` is the current text, and `setText` is
    /// called with the new text, during this call, on frames the user changed it. The text passed to `setText`
    /// is valid only during that call.
    ///
    /// ```cpp
    /// Carbon::TextField("Title", document.GetTitle(), [&](std::string_view title) { document.SetTitle(title); });
    /// ```
    bool TextField(std::string_view label, std::string_view text, FunctionRef<void(std::string_view)> setText,
                   const TextFieldOptions& options = {});

    /// A std::string is passed by pointer. Without this, a string would turn into a fixed buffer of its current
    /// size.
    bool TextField(std::string_view label, std::string& text, const TextFieldOptions& options = {}) = delete;

    /// Tells the text field `label` (at the current ID scope) that its text was replaced from outside while it is
    /// being edited, for example by choosing an item of a combo box. On its next call the caret moves to the end
    /// and the undo history starts over. Does nothing when the field is not being edited.
    void ReloadTextField(std::string_view label);

    /// Fills `selection` with the caret and selection of the text field `label` (at the current ID scope) and
    /// returns true while that field is being edited; returns false otherwise. Components built around a text
    /// field use it to act on keys at the ends of the text, such as Backspace with the caret at the start.
    bool GetTextFieldSelection(std::string_view label, TextFieldSelection* selection);

    /// Sets the caret and selection of the text field `label` (at the current ID scope), as byte offsets into its
    /// text; they are moved back onto character boundaries inside the text. Takes effect on the field's next call
    /// while it is being edited, in this frame or the next, so it can be called in the frame that gives the field
    /// focus: a component that turns into a field on a click selects the field's whole text this way.
    void SetTextFieldSelection(std::string_view label, const TextFieldSelection& selection);
} // namespace Carbon
