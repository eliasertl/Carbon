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
    /// Options of TextArea. All fields are optional.
    struct TextAreaOptions
    {
        /// Shown in a faint color while the text area is empty. Defaults to the label.
        std::string_view Placeholder = {};
        /// Width of the text area; the text wraps inside it. A text area does not fit its content's width, so Fit
        /// is the default, a fixed 240 points.
        Size Width = Size::Fixed(240.0f);
        /// Height of the visible area; longer text scrolls. Fit makes the text area grow with its text instead.
        Size Height = Size::Fixed(96.0f);
        Carbon::ControlSize ControlSize = Carbon::ControlSize::Regular;
        bool Disabled = false;
        /// Longest text the user can enter, in characters (a line break is one); 0 means unlimited.
        size_t MaxLength = 0;
        /// Tab inserts a tab character instead of moving the focus to the next control; Ctrl+Tab then moves it.
        bool AcceptsTab = false;
    };

    /// A text area editing several lines of plain text (UTF-8). Lines wrap at the width of the area, and text
    /// longer than the area scrolls inside it. Returns true on frames the text changed.
    ///
    /// Supports what TextField does (a caret, selection by mouse and keyboard, the clipboard, undo and redo),
    /// plus Return for a new line, the up and down arrows to move between lines, Home and End for the start and
    /// end of a line, and Page Up and Page Down. Tab moves the focus unless `AcceptsTab` is set; Escape gives up
    /// focus.
    ///
    /// The label identifies the text area and serves as its placeholder; put a Text next to it for a visible
    /// title.
    bool TextArea(std::string_view label, std::string* text, const TextAreaOptions& options = {});

    /// A text area editing a zero-terminated UTF-8 string in a fixed buffer owned by the caller, as the matching
    /// TextField does: the text takes all but the last byte, more typing is cut off at the last whole character
    /// that fits, and nothing is written outside the buffer.
    bool TextArea(std::string_view label, std::span<char> buffer, const TextAreaOptions& options = {});

    /// A text area for text the caller stores in its own way: `text` is the current text, and `setText` is called
    /// with the new text, during this call, on frames the user changed it.
    bool TextArea(std::string_view label, std::string_view text, FunctionRef<void(std::string_view)> setText,
                  const TextAreaOptions& options = {});

    /// A std::string is passed by pointer. Without this, a string would turn into a fixed buffer of its current
    /// size.
    bool TextArea(std::string_view label, std::string& text, const TextAreaOptions& options = {}) = delete;
} // namespace Carbon
