#pragma once

#include <cstddef>
#include <string>

namespace Carbon
{
    struct Context;
}

namespace Carbon::Internal
{
    class TextEditor;

    /// Limits and permissions of the text being edited.
    struct TextInputOptions
    {
        /// Longest text in characters, and largest in bytes; 0 means unlimited.
        size_t MaxLength = 0;
        size_t MaxBytes = 0;
        /// Copy and cut reach the clipboard (false for secure fields).
        bool CanCopy = true;
    };

    /// Applies the editing input that every text control shares, in this order: Left and Right (with Shift to
    /// select, with the shortcut modifier or Alt by word), Backspace and Delete (by word with the same modifiers),
    /// select all, copy, cut, paste, undo, redo, and the typed characters. Keys whose meaning differs between
    /// controls (Up, Down, Home, End, Enter, Tab, Escape) are left to the caller. Returns true when the text
    /// changed.
    bool ApplyTextInput(Context& context, TextEditor& editor, std::string& text, const TextInputOptions& options);
} // namespace Carbon::Internal
