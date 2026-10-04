#pragma once

#include <cstddef>
#include <string>
#include <string_view>

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

    /// Tells the text field `label` (at the current ID scope) that its text was replaced from outside while it is
    /// being edited, for example by choosing an item of a combo box. On its next call the caret moves to the end
    /// and the undo history starts over. Does nothing when the field is not being edited.
    void ReloadTextField(std::string_view label);
} // namespace Carbon
